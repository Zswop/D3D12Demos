#include "DXRPathTracer.h"
#include "DXRHelper.h"
#include "DX12_Helpers.h"
#include "..\\Utility.h"
#include "Graphics\\Skybox.h"

namespace Framework
{

static const uint64 MaxPathLength = 8;

struct GeometryInfo
{
	uint32 VtxOffset;
	uint32 IdxOffset;
	uint32 MaterialIdx;
	uint32 PadTo16Bytes;
};

struct HitGroupRecord
{
	ShaderIdentifier ID;
};

StaticAssert_(sizeof(HitGroupRecord) % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0);

struct RayTraceConstants
{
	Float4x4 InvViewProjection;

	Float3 SunDirectionWS;
	float CosSunAngularRadius = 0.0f;
	Float3 SunIlluminance;
	float SinSunAngularRadius = 0.0f;
	Float3 SunLuminance;
	uint32 Padding = 0;

	Float3 CameraPosWS;
	uint32 CurrSampleIdx = 0;

	uint32 TotalNumPixels = 0;
	uint32 SqrtNumSamples;
	uint32 MaxPathLength;
	uint32 MaxAnyHitPathLength;

	uint32 VtxBufferIdx = uint32(-1);
	uint32 IdxBufferIdx = uint32(-1);
	uint32 GeometryInfoBufferIdx = uint32(-1);
	uint32 MaterialBufferIdx = uint32(-1);
	uint32 SkyTextureIdx = uint32(-1);
	uint32 NumLights = 0;
};

enum RayTraceRootParams : uint32
{
	RayTraceParams_StandardDescriptors,
	RayTraceParams_SceneDescriptor,
	RayTraceParams_UAVDescriptor,
	RayTraceParams_CBuffer,
	RayTraceParams_LightCBuffer,

	NumRTRootParams
};

DXRPathTracer::DXRPathTracer()
{
}

void DXRPathTracer::Initialize(const Model* sceneModel)
{
	currentModel = sceneModel;

	rayTraceLib = CompileFromFile(L"Shaders\\RayTrace.hlsl", nullptr, ShaderType::Library);

	// RayTrace root signature
	{
		D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
		uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRanges[0].NumDescriptors = 1;
		uavRanges[0].BaseShaderRegister = 0;
		uavRanges[0].RegisterSpace = 0;
		uavRanges[0].OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER1 rootParameters[NumRTRootParams] = {};

		// Standard SRV descriptors
		rootParameters[RayTraceParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[RayTraceParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[RayTraceParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[RayTraceParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		// Acceleration structure SRV descriptor
		rootParameters[RayTraceParams_SceneDescriptor].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParameters[RayTraceParams_SceneDescriptor].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[RayTraceParams_SceneDescriptor].Descriptor.ShaderRegister = 0;
		rootParameters[RayTraceParams_SceneDescriptor].Descriptor.RegisterSpace = 200;

		// UAV descriptor
		rootParameters[RayTraceParams_UAVDescriptor].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[RayTraceParams_UAVDescriptor].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[RayTraceParams_UAVDescriptor].DescriptorTable.pDescriptorRanges = uavRanges;
		rootParameters[RayTraceParams_UAVDescriptor].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

		// CBuffer
		rootParameters[RayTraceParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[RayTraceParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[RayTraceParams_CBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[RayTraceParams_CBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[RayTraceParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		// LightCBuffer
		rootParameters[RayTraceParams_LightCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[RayTraceParams_LightCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[RayTraceParams_LightCBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[RayTraceParams_LightCBuffer].Descriptor.ShaderRegister = 1;
		rootParameters[RayTraceParams_LightCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
		staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Anisotropic, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
		staticSamplers[1] = DX12::GetStaticSamplerState(SamplerState::LinearClamp, 1, 0, D3D12_SHADER_VISIBILITY_ALL);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
		rootSignatureDesc.pStaticSamplers = staticSamplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		DX12::CreateRootSignature(&rtRootSignature, rootSignatureDesc);
	}

	buildAccelStructure = true;
}

void DXRPathTracer::Shutdown()
{
	rtTarget.Shutdown();
	DX12::Release(rtRootSignature);
	rtBottomLevelAccelStructure.Shutdown();
	rtTopLevelAccelStructure.Shutdown();
	rtRayGenTable.Shutdown();
	rtHitTable.Shutdown();
	rtMissTable.Shutdown();
	rtGeoInfoBuffer.Shutdown();
	currentModel = nullptr;
}

void DXRPathTracer::CreatePSOs()
{
	StateObjectBuilder builder;
	builder.Init(12);

	{
		// DXIL library sub-object containing all of our code
		D3D12_DXIL_LIBRARY_DESC dxilDesc = { };
		dxilDesc.DXILLibrary = rayTraceLib.ByteCode();
		builder.AddSubObject(dxilDesc);
	}

	{
		// Primary hit group
		D3D12_HIT_GROUP_DESC hitDesc = { };
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ClosestHitShader";
		hitDesc.HitGroupExport = L"HitGroup";
		builder.AddSubObject(hitDesc);
	}

	{
		// Primary alpha-test hit group
		D3D12_HIT_GROUP_DESC hitDesc = { };
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ClosestHitShader";
		hitDesc.AnyHitShaderImport = L"AnyHitShader";
		hitDesc.HitGroupExport = L"AlphaTestHitGroup";
		builder.AddSubObject(hitDesc);
	}

	{
		// Shadow hit group
		D3D12_HIT_GROUP_DESC hitDesc = { };
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ShadowHitShader";
		hitDesc.HitGroupExport = L"ShadowHitGroup";
		builder.AddSubObject(hitDesc);
	}

	{
		// Shadow alpha-test hit group
		D3D12_HIT_GROUP_DESC hitDesc = { };
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ShadowHitShader";
		hitDesc.AnyHitShaderImport = L"ShadowAnyHitShader";
		hitDesc.HitGroupExport = L"ShadowAlphaTestHitGroup";
		builder.AddSubObject(hitDesc);
	}

	{
		D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = { };
		// float2 barycentrics;
		shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);                     
		// float3 radiance + float roughness + uint pathLength + uint pixelIdx + uint setIdx + bool IsDiffuse
		shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(float) + 4 * sizeof(uint32);   
		builder.AddSubObject(shaderConfig);
	}

	{
		// Global root signature with all of our normal bindings
		D3D12_GLOBAL_ROOT_SIGNATURE globalRSDesc = { };
		globalRSDesc.pGlobalRootSignature = rtRootSignature;
		builder.AddSubObject(globalRSDesc);
	}

	{
		// The path tracer is recursive, so set the max recursion depth to the max path length
		D3D12_RAYTRACING_PIPELINE_CONFIG configDesc = { };
		configDesc.MaxTraceRecursionDepth = MaxPathLength;
		builder.AddSubObject(configDesc);
	}

	rtPSO = builder.CreateStateObject(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Get shader identifiers (for making shader records)
	ID3D12StateObjectProperties* psoProps = nullptr;
	rtPSO->QueryInterface(IID_PPV_ARGS(&psoProps));

	const void* rayGenID = psoProps->GetShaderIdentifier(L"RaygenShader");
	const void* hitGroupID = psoProps->GetShaderIdentifier(L"HitGroup");
	const void* alphaTestHitGroupID = psoProps->GetShaderIdentifier(L"AlphaTestHitGroup");
	const void* shadowHitGroupID = psoProps->GetShaderIdentifier(L"ShadowHitGroup");
	const void* shadowAlphaTestHitGroupID = psoProps->GetShaderIdentifier(L"ShadowAlphaTestHitGroup");
	const void* missID = psoProps->GetShaderIdentifier(L"MissShader");
	const void* shadowMissID = psoProps->GetShaderIdentifier(L"ShadowMissShader");

	// Make our shader tables
	{
		ShaderIdentifier rayGenRecords[1] = { ShaderIdentifier(rayGenID) };
		
		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(ShaderIdentifier);
		sbInit.NumElements = ArraySize_(rayGenRecords);
		sbInit.InitData = rayGenRecords;
		sbInit.ShaderTable = true;
		sbInit.Name = L"Ray Gen Shader Table";
		rtRayGenTable.Initialize(sbInit);
	}

	{
		ShaderIdentifier missRecords[2] = { ShaderIdentifier(missID), ShaderIdentifier(shadowMissID) };

		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(ShaderIdentifier);
		sbInit.NumElements = ArraySize_(missRecords);
		sbInit.InitData = missRecords;
		sbInit.ShaderTable = true;
		sbInit.Name = L"Miss Shader Table";
		rtMissTable.Initialize(sbInit);
	}

	{
		const uint64 numMeshes = currentModel->NumMeshes();

		Array<HitGroupRecord> hitGroupRecords(numMeshes * 2);
		for (uint64 i = 0; i < numMeshes; ++i)
		{
			// Use the alpha test hit group (with an any hit shader) if the material has an opacity map
			const Mesh& mesh = currentModel->Meshes()[i];
			Assert_(mesh.NumMeshParts() == 1);
			const uint32 materialIdx = mesh.MeshParts()[0].MaterialIdx;
			const MeshMaterial& material = currentModel->Materials()[materialIdx];
			const bool alphaTest = material.AlphaTest();

			hitGroupRecords[i * 2 + 0].ID = alphaTest ? ShaderIdentifier(alphaTestHitGroupID) : ShaderIdentifier(hitGroupID);
			hitGroupRecords[i * 2 + 1].ID = alphaTest ? ShaderIdentifier(shadowAlphaTestHitGroupID) : ShaderIdentifier(shadowHitGroupID);
		}

		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(HitGroupRecord);
		sbInit.NumElements = hitGroupRecords.Size();
		sbInit.InitData = hitGroupRecords.Data();
		sbInit.ShaderTable = true;
		sbInit.Name = L"Hit Shader Table";
		rtHitTable.Initialize(sbInit);
	}

	DX12::Release(psoProps);
}

void DXRPathTracer::DestroyPSOs()
{
	DX12::DeferredRelease(rtPSO);
}

void DXRPathTracer::CreateRenderTarget(uint32 width, uint32 height)
{
	if (rtTarget.RTV.ptr == 0 || rtTarget.Width() != width ||rtTarget.Height() != height)
	{
		RenderTextureInit rtInit;
		rtInit.Width = width;
		rtInit.Height = height;
		rtInit.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		rtInit.CreateUAV = true;
		rtInit.Name = L"RT Target";
		rtTarget.Initialize(rtInit);
	}
}

void DXRPathTracer::Render(ID3D12GraphicsCommandList4* cmdList, const Camera& camera, const RayTraceData& rtData)
{
	if (buildAccelStructure)
		BuildRTAccelerationStructure();

	if (rtData.ShouldRestartPathTrace)
		rtCurrSampleIdx = 0;

	// Don't keep tracing rays if we've hit our maximum per-pixel sample count	
	if (rtCurrSampleIdx >= uint32(rtData.SqrtNumSamples * rtData.SqrtNumSamples))
		return;

	PIXMarker marker(cmdList, "DXR RayTracing");

	cmdList->SetComputeRootSignature(rtRootSignature);

	DX12::BindStandardDescriptorTable(cmdList, RayTraceParams_StandardDescriptors, CmdListMode::Compute);

	cmdList->SetComputeRootShaderResourceView(RayTraceParams_SceneDescriptor, rtTopLevelAccelStructure.GPUAddress);

	DX12::BindTempDescriptorTable(cmdList, &rtTarget.UAV, 1, RayTraceParams_UAVDescriptor, CmdListMode::Compute);

	RayTraceConstants rtConstants;
	rtConstants.InvViewProjection = Float4x4::Invert(camera.ViewProjectionMatrix());

	const SkyCache* skyCache = rtData.SkyCache;
	rtConstants.SunDirectionWS = skyCache->SunDirection;
	rtConstants.SunIlluminance = skyCache->SunIlluminance;
	rtConstants.CosSunAngularRadius = std::cos(DegToRad(skyCache->SunSize));
	rtConstants.SinSunAngularRadius = std::sin(DegToRad(skyCache->SunSize));
	rtConstants.SunLuminance = skyCache->SunLuminance;

	rtConstants.CameraPosWS = camera.Position();
	rtConstants.CurrSampleIdx = rtCurrSampleIdx;

	rtConstants.TotalNumPixels = uint32(rtTarget.Width()) * uint32(rtTarget.Height());
	rtConstants.SqrtNumSamples = rtData.SqrtNumSamples;
	rtConstants.MaxPathLength = rtData.MaxPathLength;
	rtConstants.MaxAnyHitPathLength = rtData.MaxAnyHitPathLength;

	rtConstants.VtxBufferIdx = currentModel->VertexBuffer().SRV;
    rtConstants.IdxBufferIdx = currentModel->IndexBuffer().SRV;
    rtConstants.GeometryInfoBufferIdx = rtGeoInfoBuffer.SRV;
    rtConstants.MaterialBufferIdx = rtData.MaterialBuffer->SRV;
	rtConstants.SkyTextureIdx = rtData.EnvCubMap->SRV;
	rtConstants.NumLights = rtData.NumLights;

	DX12::BindTempConstantBuffer(cmdList, rtConstants, RayTraceParams_CBuffer, CmdListMode::Compute);

	rtData.SpotLightBuffer->SetAsComputeRootParameter(cmdList, RayTraceParams_LightCBuffer);

	rtTarget.MakeWritableUAV(cmdList);

	cmdList->SetPipelineState1(rtPSO);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.HitGroupTable = rtHitTable.ShaderTable();
	dispatchDesc.MissShaderTable = rtMissTable.ShaderTable();
	dispatchDesc.RayGenerationShaderRecord = rtRayGenTable.ShaderRecord(0);
	dispatchDesc.Width = uint32(rtTarget.Width());
	dispatchDesc.Height = uint32(rtTarget.Height());
	dispatchDesc.Depth = 1;

	DX12::CmdList->DispatchRays(&dispatchDesc);

	rtTarget.MakeReadableUAV(cmdList);

	rtCurrSampleIdx += 1;
}

void DXRPathTracer::BuildRTAccelerationStructure()
{
	const FormattedBuffer& idxBuffer = currentModel->IndexBuffer();
	const StructuredBuffer& vtxBuffer = currentModel->VertexBuffer();

	const uint64 numMeshes = currentModel->NumMeshes();
	Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(numMeshes);

	const uint32 numGeometries = uint32(geometryDescs.Size());
	Array<GeometryInfo> geoInfoBufferData(numGeometries);

	for (uint64 meshIdx = 0; meshIdx < numMeshes; ++meshIdx)
	{
		const Mesh& mesh = currentModel->Meshes()[meshIdx];
		Assert_(mesh.NumMeshParts() == 1);

		const uint32 materialIdx = mesh.MeshParts()[0].MaterialIdx;
		const MeshMaterial& material = currentModel->Materials()[materialIdx];
		const bool opaque = material.Textures[uint32(MaterialTextures::Opacity)] == nullptr;

		D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = geometryDescs[meshIdx];
		geometryDesc = { };
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = idxBuffer.GPUAddress + mesh.IndexOffset() * idxBuffer.Stride;
		geometryDesc.Triangles.IndexCount = uint32(mesh.NumIndices());
		geometryDesc.Triangles.IndexFormat = idxBuffer.Format;
		geometryDesc.Triangles.Transform3x4 = 0;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.VertexCount = uint32(mesh.NumVertices());
		geometryDesc.Triangles.VertexBuffer.StartAddress = vtxBuffer.GPUAddress + mesh.VertexOffset() * vtxBuffer.Stride;
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = vtxBuffer.Stride;
		geometryDesc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

		GeometryInfo& geoInfo = geoInfoBufferData[meshIdx];
		geoInfo = { };
		geoInfo.VtxOffset = uint32(mesh.VertexOffset());
		geoInfo.IdxOffset = uint32(mesh.IndexOffset());
		geoInfo.MaterialIdx = mesh.MeshParts()[0].MaterialIdx;
	}

	// Get required sizes for an acceleration structure
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};

	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		prebuildInfoDesc.pGeometryDescs = nullptr;
		prebuildInfoDesc.NumDescs = 1;
		DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topLevelPrebuildInfo);
	}

	Assert_(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};

	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.pGeometryDescs = geometryDescs.Data();
		prebuildInfoDesc.NumDescs = numGeometries;
		DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomLevelPrebuildInfo);
	}

	Assert_(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	RawBuffer scratchBuffer;

	{
		RawBufferInit bufferInit;
		bufferInit.NumElements = Max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes) / RawBuffer::Stride;
		bufferInit.CreateUAV = true;
		bufferInit.InitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		bufferInit.Name = L"RT Scratch Buffer";
		scratchBuffer.Initialize(bufferInit);
	}

	{
		RawBufferInit bufferInit;
		bufferInit.NumElements = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
		bufferInit.CreateUAV = true;
		bufferInit.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		bufferInit.Name = L"RT Bottom Level Accel Structure";
		rtBottomLevelAccelStructure.Initialize(bufferInit);
	}

	{
		RawBufferInit bufferInit;
		bufferInit.NumElements = topLevelPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
		bufferInit.CreateUAV = true;
		bufferInit.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		bufferInit.Name = L"RT Top Level Accel Structure";
		rtTopLevelAccelStructure.Initialize(bufferInit);
	}

	// Create an instance desc for the bottom-level acceleration structure.
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
	instanceDesc.InstanceMask = 1;
	instanceDesc.AccelerationStructure = rtBottomLevelAccelStructure.GPUAddress;

	TempBuffer instanceBuffer = DX12::TempStructuredBuffer(1, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), false);
	memcpy(instanceBuffer.CPUAddress, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	{
		bottomLevelBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelBuildDesc.Inputs.Flags = buildFlags;
		bottomLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelBuildDesc.Inputs.NumDescs = numGeometries;
		bottomLevelBuildDesc.Inputs.pGeometryDescs = geometryDescs.Data();
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
		bottomLevelBuildDesc.DestAccelerationStructureData = rtBottomLevelAccelStructure.GPUAddress;
	}

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
	{
		topLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelBuildDesc.Inputs.NumDescs = 1;
		topLevelBuildDesc.Inputs.pGeometryDescs = nullptr;
		topLevelBuildDesc.Inputs.InstanceDescs = instanceBuffer.GPUAddress;
		topLevelBuildDesc.DestAccelerationStructureData = rtTopLevelAccelStructure.GPUAddress;;
		topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
	}

	{
		DX12::CmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		rtBottomLevelAccelStructure.UAVBarrier(DX12::CmdList);

		DX12::CmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
		rtTopLevelAccelStructure.UAVBarrier(DX12::CmdList);
	}

	scratchBuffer.Shutdown();

	{
		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(GeometryInfo);
		sbInit.NumElements = numGeometries;
		sbInit.Name = L"Geometry Info Buffer";
		sbInit.InitData = geoInfoBufferData.Data();
		rtGeoInfoBuffer.Initialize(sbInit);
	}

	buildAccelStructure = false;
	lastBuildAccelStructureFrame = DX12::CurrentCPUFrame;
}

}