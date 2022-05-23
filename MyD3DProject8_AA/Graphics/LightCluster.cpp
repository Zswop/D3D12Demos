#include "LightCluster.h"
#include "Camera.h"

namespace Framework
{

static const uint64 NumZTiles = 16;
static const uint64 NumConeSides = 16;
static const uint64 ClusterTileSize = 16;
static const uint64 SpotLightElementsPerCluster = 1;
static const float SpotLightIntensityFactor = 25.0f;

struct ClusterBounds
{
	Float3 Position;
	Quaternion Orientation;
	Float3 Scale;
	Uint2 ZBounds;
};

struct ClusterConstants
{
	Float4x4 ViewProjection;
	Float4x4 InvProjection;
	float NearClip = 0.0f;
	float FarClip = 0.0f;
	float InvClipRange = 0.0f;
	uint32 NumXTiles = 0;
	uint32 NumYTiles = 0;
	uint32 NumXYTiles = 0;
	uint32 ElementsPerCluster = 0;
	uint32 InstanceOffset = 0;
	uint32 NumLights = 0;
	uint32 NumDecals = 0;

	uint32 BoundsBufferIdx = uint32(-1);
	uint32 VertexBufferIdx = uint32(-1);
	uint32 InstanceBufferIdx = uint32(-1);
};

enum ClusterRootParams : uint32
{
	ClusterParams_StandardDescriptors,
	ClusterParams_UAVDescriptors,
	ClusterParams_CBuffer,

	NumClusterRootParams,
};

struct ClusterVisConstants
{
	Float4x4 Projection;
	Float3 ViewMin;
	float NearClip = 0.0f;
	Float3 ViewMax;
	float InvClipRange = 0.0f;
	Float2 DisplaySize;
	uint32 NumXTiles = 0;
	uint32 NumXYTiles = 0;

	uint32 DecalClusterBufferIdx = uint32(-1);
	uint32 SpotLightClusterBufferIdx = uint32(-1);
};

enum ClusterVisRootParams : uint32
{
	ClusterVisParams_StandardDescriptors,
	ClusterVisParams_CBuffer,

	NumClusterVisRootParams,
};

// Returns true if a sphere intersects a capped cone defined by a direction, height, and angle
static bool SphereConeIntersection(const Float3& coneTip, const Float3& coneDir, float coneHeight,
	float coneAngle, const Float3& sphereCenter, float sphereRadius)
{
	if (Float3::Dot(sphereCenter - coneTip, coneDir) > coneHeight + sphereRadius)
		return false;

	float cosHalfAngle = std::cos(coneAngle * 0.5f);
	float sinHalfAngle = std::sin(coneAngle * 0.5f);

	Float3 v = sphereCenter - coneTip;
	float a = Float3::Dot(v, coneDir);
	float b = a * sinHalfAngle / cosHalfAngle;
	float c = std::sqrt(Float3::Dot(v, v) - a * a);
	float d = c - b;
	float e = d * cosHalfAngle;

	return e < sphereRadius;
}

LightCluster::LightCluster()
{
};

void LightCluster::Initialize(const Model* sceneModel, int width, int height)
{
	model = sceneModel;

	// Initialize the spotlight data used for rendering
	const uint64 numSpotLights = model->SpotLights().Size();
	spotLights.Init(numSpotLights);

	for (uint64 i = 0; i < numSpotLights; ++i)
	{
		const ModelSpotLight& srcLight = model->SpotLights()[i];

		SpotLight& spotLight = spotLights[i];
		spotLight.Position = srcLight.Position;
		spotLight.Direction = -srcLight.Direction;
		spotLight.Intensity = srcLight.Intensity * SpotLightIntensityFactor;
		spotLight.AngularAttenuationX = std::cos(srcLight.AngularAttenuation.x * 0.5f);
		spotLight.AngularAttenuationY = std::cos(srcLight.AngularAttenuation.y * 0.5f);
		spotLight.Range = 7.5f;
	}

	{
		// Spot light bounds and instance buffers
		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(ClusterBounds);
		sbInit.NumElements = MaxSpotLights;
		sbInit.Dynamic = true;
		sbInit.CPUAccessible = true;
		spotLightBoundsBuffer.Initialize(sbInit);

		sbInit.Stride = sizeof(uint32);
		spotLightInstanceBuffer.Initialize(sbInit);
	}

	numXTiles = (width + (ClusterTileSize - 1)) / ClusterTileSize;
	numYTiles = (height + (ClusterTileSize - 1)) / ClusterTileSize;
	const uint64 numXYZTiles = numXTiles * numYTiles * NumZTiles;

	{
		// SpotLight cluster bitmask buffer
		RawBufferInit rbInit;
		rbInit.NumElements = numXYZTiles * SpotLightElementsPerCluster;
		rbInit.CreateUAV = true;
		rbInit.InitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		rbInit.Name = L"Spot Light Cluster Buffer";
		spotLightClusterBuffer.Initialize(rbInit);
	}

	{
		CompileOptions opts;
		opts.Add("FrontFace_", 1);
		opts.Add("BackFace_", 0);
		opts.Add("Intersecting_", 0);

		clusterVS = CompileFromFile(L"Shaders\\Clusters.hlsl", "ClusterVS", ShaderType::Vertex, opts);
		clusterFrontFacePS = CompileFromFile(L"Shaders\\Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);

		opts.Reset();
		opts.Add("FrontFace_", 0);
		opts.Add("BackFace_", 1);
		opts.Add("Intersecting_", 0);
		clusterBackFacePS = CompileFromFile(L"Shaders\\Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);

		opts.Reset();
		opts.Add("FrontFace_", 0);
		opts.Add("BackFace_", 0);
		opts.Add("Intersecting_", 1);
		clusterIntersectingPS = CompileFromFile(L"Shaders\\Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);
	}

	MakeConeGeometry(NumConeSides, spotLightClusterVtxBuffer, spotLightClusterIdxBuffer, coneVertices);

	// Compile cluster visualization shaders
	fullScreenTriVS = CompileFromFile(L"Shaders\\FullScreenTriangle.hlsl", "FullScreenTriangleVS", ShaderType::Vertex);
	clusterVisPS = CompileFromFile(L"Shaders\\ClusterVisualizer.hlsl", "ClusterVisualizerPS", ShaderType::Pixel);

	{
		// Cluster root signature
		D3D12_ROOT_PARAMETER1 rootParameters[NumClusterRootParams] = { };
		rootParameters[ClusterParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ClusterParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[ClusterParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[ClusterParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = { };
		uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRanges[0].NumDescriptors = 1;
		uavRanges[0].BaseShaderRegister = 0;
		uavRanges[0].RegisterSpace = 0;
		uavRanges[0].OffsetInDescriptorsFromTableStart = 0;

		rootParameters[ClusterParams_UAVDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ClusterParams_UAVDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ClusterParams_UAVDescriptors].DescriptorTable.pDescriptorRanges = uavRanges;
		rootParameters[ClusterParams_UAVDescriptors].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

		rootParameters[ClusterParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[ClusterParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[ClusterParams_CBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[ClusterParams_CBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[ClusterParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		DX12::CreateRootSignature(&clusterRS, rootSignatureDesc);
	}

	{
		// Cluster visualization root signature
		D3D12_ROOT_PARAMETER1 rootParameters[NumClusterVisRootParams] = {};

		// Standard SRV descriptors
		rootParameters[ClusterVisParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ClusterVisParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ClusterVisParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[ClusterVisParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		// CBuffer
		rootParameters[ClusterVisParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[ClusterVisParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ClusterVisParams_CBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[ClusterVisParams_CBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[ClusterVisParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		DX12::CreateRootSignature(&clusterVisRS, rootSignatureDesc);
	}
}

void LightCluster::Shutdown()
{
	DestroyPSOs();

	spotLightBoundsBuffer.Shutdown();
	spotLightClusterBuffer.Shutdown();
	spotLightInstanceBuffer.Shutdown();

	spotLightClusterVtxBuffer.Shutdown();
	spotLightClusterIdxBuffer.Shutdown();

	DX12::Release(clusterRS);
	DX12::Release(clusterVisRS);
}

void LightCluster::CreatePSOs(DXGI_FORMAT rtFormat)
{
	{
		// Clustering PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
		psoDesc.pRootSignature = clusterRS;
		psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 0;
		psoDesc.VS = clusterVS.ByteCode();

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		D3D12_CONSERVATIVE_RASTERIZATION_MODE crMode = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;

		psoDesc.PS = clusterFrontFacePS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
		psoDesc.RasterizerState.ConservativeRaster = crMode;
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterFrontFacePSO)));

		psoDesc.PS = clusterBackFacePS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
		psoDesc.RasterizerState.ConservativeRaster = crMode;
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterBackFacePSO)));

		psoDesc.PS = clusterIntersectingPS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
		psoDesc.RasterizerState.ConservativeRaster = crMode;
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterIntersectingPSO)));

		clusterFrontFacePSO->SetName(L"Cluster Front-Face PSO");
		clusterBackFacePSO->SetName(L"Cluster Back-Face PSO");
		clusterIntersectingPSO->SetName(L"Cluster Intersecting PSO");
	}

	{
		// Cluster visualizer PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = clusterVisRS;
		psoDesc.VS = fullScreenTriVS.ByteCode();
		psoDesc.PS = clusterVisPS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::AlphaBlend);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = rtFormat;
		psoDesc.SampleDesc.Count = 1;
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterVisPSO)));
	}
}

void LightCluster::DestroyPSOs()
{
	DX12::DeferredRelease(clusterFrontFacePSO);
	DX12::DeferredRelease(clusterBackFacePSO);
	DX12::DeferredRelease(clusterIntersectingPSO);

	DX12::DeferredRelease(clusterVisPSO);
}

void LightCluster::UpdateLights(const Camera& camera)
{
	const uint64 numSpotLights = Min<uint64>(spotLights.Size(), MaxSpotLights);

	// This is an additional scale factor that's needed to make sure that our polygonal bounding cone
	// fully encloses the actual cone representing the light's area of influence
	const float inRadius = std::cos(Pi / NumConeSides);
	const float scaleCorrection = 1 / inRadius;

	const Float4x4 viewMatrix = camera.ViewMatrix();
	const float nearClip = camera.NearClip();
	const float farClip = camera.FarClip();
	const float zRange = farClip - nearClip;
	const Float3 cameraPos = camera.Position();
	const uint64 numConeVerts = coneVertices.Size();

	// Come up with a bounding sphere that surrounds the near clipping plane. We'll test this sphere
	// for intersection with the spot light's bounding cone, and use that to over-estimate if the bounding
	// geometry will end up getting clipped by the camera's near clipping plane
	Float3 nearClipCenter = cameraPos + nearClip * camera.Forward();
	Float4x4 invViewProjection = Float4x4::Invert(camera.ViewProjectionMatrix());
	Float3 nearTopRight = Float3::Transform(Float3(1.0f, 1.0f, 0.0f), invViewProjection);
	float nearClipRadius = Float3::Length(nearTopRight - nearClipCenter);

	ClusterBounds* boundsData = spotLightBoundsBuffer.Map<ClusterBounds>();
	bool intersectsCamera[MaxSpotLights] = { };

	// Update the light bounds buffer
	for (uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; spotLightIdx++)
	{
		const SpotLight& spotLight = spotLights[spotLightIdx];
		const ModelSpotLight& srcSpotLight = model->SpotLights()[spotLightIdx];

		ClusterBounds bounds;
		bounds.Position = spotLight.Position;
		bounds.Orientation = srcSpotLight.Orientation;
		bounds.Scale.x = bounds.Scale.y = std::tan(srcSpotLight.AngularAttenuation.y / 2.0f) * spotLight.Range * scaleCorrection;
		bounds.Scale.z = spotLight.Range;

		float minZ = FloatMax;
		float maxZ = -FloatMax;
		for (uint64 i = 0; i < numConeVerts; ++i)
		{
			Float3 coneVert = coneVertices[i] * bounds.Scale;
			coneVert = Float3::Transform(coneVert, bounds.Orientation);
			coneVert += bounds.Position;

			float vertZ = Float3::Transform(coneVert, viewMatrix).z;
			minZ = Min(minZ, vertZ);
			maxZ = Max(maxZ, vertZ);
		}

		minZ = Saturate((minZ - nearClip) / zRange);
		maxZ = Saturate((maxZ - nearClip) / zRange);

		bounds.ZBounds.x = uint32(minZ * NumZTiles);
		bounds.ZBounds.y = uint32(maxZ * NumZTiles);

		boundsData[spotLightIdx] = bounds;
		intersectsCamera[spotLightIdx] = SphereConeIntersection(spotLight.Position, srcSpotLight.Direction, spotLight.Range,
			srcSpotLight.AngularAttenuation.y, nearClipCenter, nearClipRadius);
	}

	numIntersectingSpotLights = 0;
	uint32* instanceData = spotLightInstanceBuffer.Map<uint32>();

	for (uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; ++spotLightIdx)
		if (intersectsCamera[spotLightIdx])
			instanceData[numIntersectingSpotLights++] = uint32(spotLightIdx);

	uint64 offset = numIntersectingSpotLights;
	for (uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; spotLightIdx++)
		if (intersectsCamera[spotLightIdx] == false)
			instanceData[offset++] = uint32(spotLightIdx);
}

void LightCluster::RenderClusters(ID3D12GraphicsCommandList* cmdList, const Camera& camera)
{
	PIXMarker marker(cmdList, "Cluster Update");

	spotLightClusterBuffer.MakeWritable(cmdList);

	{
		// Clear spot light clusters
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptors[1] = { spotLightClusterBuffer.UAV };
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = DX12::TempDescriptorTable(cpuDescriptors, ArraySize_(cpuDescriptors));

		uint32 values[4] = { };
		cmdList->ClearUnorderedAccessViewUint(gpuHandle, cpuDescriptors[0], spotLightClusterBuffer.InternalBuffer.Resource, values, 0, nullptr);

		ClusterConstants clusterConstants;
		clusterConstants.ViewProjection = camera.ViewProjectionMatrix();
		clusterConstants.InvProjection = Float4x4::Invert(camera.ProjectionMatrix());
		clusterConstants.NearClip = camera.NearClip();
		clusterConstants.FarClip = camera.FarClip();
		clusterConstants.InvClipRange = 1.0f / (camera.FarClip() - camera.NearClip());
		clusterConstants.NumXTiles = uint32(numXTiles);
		clusterConstants.NumYTiles = uint32(numYTiles);
		clusterConstants.NumXYTiles = uint32(numXTiles * numYTiles);
		clusterConstants.InstanceOffset = 0;
		clusterConstants.NumLights = Min<uint32>(uint32(spotLights.Size()), uint32(MaxSpotLights));
		clusterConstants.NumDecals = 0;

		cmdList->OMSetRenderTargets(0, nullptr, false, nullptr);

		DX12::SetViewport(cmdList, numXTiles, numYTiles);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmdList->SetGraphicsRootSignature(clusterRS);

		DX12::BindStandardDescriptorTable(cmdList, ClusterParams_StandardDescriptors, CmdListMode::Graphics);

		D3D12_INDEX_BUFFER_VIEW ibView = spotLightClusterIdxBuffer.IBView();
		cmdList->IASetIndexBuffer(&ibView);

		clusterConstants.InstanceOffset = 0;
		clusterConstants.ElementsPerCluster = uint32(SpotLightElementsPerCluster);
		clusterConstants.BoundsBufferIdx = spotLightBoundsBuffer.SRV;
		clusterConstants.VertexBufferIdx = spotLightClusterVtxBuffer.SRV;
		clusterConstants.InstanceBufferIdx = spotLightInstanceBuffer.SRV;
		DX12::BindTempConstantBuffer(cmdList, clusterConstants, ClusterParams_CBuffer, CmdListMode::Graphics);

		D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { spotLightClusterBuffer.UAV };
		DX12::BindTempDescriptorTable(cmdList, uavs, ArraySize_(uavs), ClusterParams_UAVDescriptors, CmdListMode::Graphics);

		const uint64 numLightsToRender = Min<uint64>(spotLights.Size(), MaxSpotLights);
		Assert_(numIntersectingSpotLights <= numLightsToRender);
		const uint64 numNonIntersecting = numLightsToRender - numIntersectingSpotLights;

		// Render back faces for decals that intersect with the camera
		cmdList->SetPipelineState(clusterIntersectingPSO);
		cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numIntersectingSpotLights), 0, 0, 0);

		// Now for all other lights, render the back faces followed by the front faces
		cmdList->SetPipelineState(clusterBackFacePSO);
		clusterConstants.InstanceOffset = uint32(numIntersectingSpotLights);
		DX12::BindTempConstantBuffer(cmdList, clusterConstants, ClusterParams_CBuffer, CmdListMode::Graphics);
		cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numNonIntersecting), 0, 0, 0);

		spotLightClusterBuffer.UAVBarrier(cmdList);

		cmdList->SetPipelineState(clusterFrontFacePSO);
		cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numNonIntersecting), 0, 0, 0);
	}

	spotLightClusterBuffer.MakeReadable(cmdList);
}

void LightCluster::RenderClusterVisualizer(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const Float2 displaySize)
{
	PIXMarker pixMarker(cmdList, "Cluster Visualizer");

	Float2 drawSize = displaySize * 0.375f;
	Float2 drawPos = displaySize * (0.5f + (0.5f - 0.375f) / 2);

	D3D12_VIEWPORT viewport = { };
	viewport.Width = drawSize.x;
	viewport.Height = drawSize.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = drawPos.x;
	viewport.TopLeftY = drawPos.y;

	D3D12_RECT scissorRect = { };
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = uint32(displaySize.x);
	scissorRect.bottom = uint32(displaySize.y);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->SetGraphicsRootSignature(clusterVisRS);
	cmdList->SetPipelineState(clusterVisPSO);

	DX12::BindStandardDescriptorTable(cmdList, ClusterVisParams_StandardDescriptors, CmdListMode::Graphics);

	Float4x4 invProjection = Float4x4::Invert(camera.ProjectionMatrix());
	Float3 farTopRight = Float3::Transform(Float3(1.0f, 1.0f, 1.0f), invProjection);
	Float3 farBottomLeft = Float3::Transform(Float3(-1.0f, -1.0f, 1.0f), invProjection);

	ClusterVisConstants clusterVisConstants;
	clusterVisConstants.Projection = camera.ProjectionMatrix();
	clusterVisConstants.ViewMin = Float3(farBottomLeft.x, farBottomLeft.y, camera.NearClip());
	clusterVisConstants.NearClip = camera.NearClip();
	clusterVisConstants.ViewMax = Float3(farTopRight.x, farTopRight.y, camera.FarClip());
	clusterVisConstants.InvClipRange = 1.0f / (camera.FarClip() - camera.NearClip());
	clusterVisConstants.DisplaySize = displaySize;
	clusterVisConstants.NumXTiles = uint32(numXTiles);
	clusterVisConstants.NumXYTiles = uint32(numXTiles * numYTiles);
	clusterVisConstants.DecalClusterBufferIdx = 0;
	clusterVisConstants.SpotLightClusterBufferIdx = spotLightClusterBuffer.SRV;
	DX12::BindTempConstantBuffer(cmdList, clusterVisConstants, ClusterVisParams_CBuffer, CmdListMode::Graphics);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetVertexBuffers(0, 0, nullptr);

	cmdList->DrawInstanced(3, 1, 0, 0);
}
}