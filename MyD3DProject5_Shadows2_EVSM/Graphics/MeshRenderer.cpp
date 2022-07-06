#include "MeshRenderer.h"

#include "Skybox.h"
#include "..\\Exceptions.h"
#include "..\\Utility.h"
#include "..\\AppSettings.h"

namespace Framework
{

static const uint64 SunShadowMapSize = 2048;

enum MainPassRootParams : uint32
{	
	MainPass_StandardDescriptors,
	MainPass_VSCBuffer,
	MainPass_PSCBuffer,
	MainPass_ShadowCBuffer,
	MainPass_MatIndexCBuffer,
	MainPass_SRVIndices,

	NumMainPassRootParams,
};

struct MeshVSConstants
{
	Float4Align Float4x4 World;
	Float4Align Float4x4 View;
	Float4Align Float4x4 WorldViewProjection;
};

struct SRVIndexConstants
{
	uint32 sunShadowMapIdx;
	uint32 MaterialTextureIndicesIdx;
	uint32 SpecularCubemapLookupIdx;
	uint32 SpecularCubemapIdx;
};

struct MaterialTextureIndices
{
	uint32 Albedo;
	uint32 Normal;
	uint32 Roughness;
	uint32 Metallic;
};

MeshRenderer::MeshRenderer()
{
}

void MeshRenderer::LoadShaders()
{
	// Load the mesh shaders
	meshVS = CompileFromFile(L"Shaders\\Mesh.hlsl", "VS", ShaderType::Vertex);

	CompileOptions opts;
	opts.Add("ShadowMapMode_", (uint32)AppSettings::CurrentSMMode);
	meshPSForward = CompileFromFile(L"Shaders\\Mesh.hlsl", "PSForward", ShaderType::Pixel, opts);
}

void MeshRenderer::CreateShadowMaps()
{
	DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

	DXGI_FORMAT smFmt = ShadowHelper::SMFormat(AppSettings::CurrentSMMode);
	bool useFilterableShadows = AppSettings::UseFilterableShadows();

	if (useFilterableShadows)
	{
		DepthBufferInit dbInit;
		dbInit.Width = SunShadowMapSize;
		dbInit.Height = SunShadowMapSize;
		dbInit.Format = depthFormat;
		dbInit.MSAASamples = 1;
		dbInit.ArraySize = 1;
		dbInit.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		dbInit.Name = L"Sun Shadow Map";
		sunShadowMap.Initialize(dbInit);

		RenderTextureInit rtInit;
		rtInit.Width = SunShadowMapSize;
		rtInit.Height = SunShadowMapSize;
		rtInit.Format = smFmt;
		rtInit.MSAASamples = 1;
		rtInit.ArraySize = NumCascades;
		rtInit.CreateUAV = true;
		rtInit.MipLevels = NumMipLevels(SunShadowMapSize, SunShadowMapSize);
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Sun Fiterable Shadow Map";
		sunFilterableShadowMap.Initialize(rtInit);
		
		rtInit.ArraySize = 1;
		rtInit.MipLevels = 0;
		rtInit.CreateUAV = false;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Temp Fiterable Shadow Map";
		tempFilterableShadowMap.Initialize(rtInit);
	}
	else
	{
		DepthBufferInit dbInit;
		dbInit.Width = SunShadowMapSize;
		dbInit.Height = SunShadowMapSize;
		dbInit.Format = depthFormat;
		dbInit.MSAASamples = 1;
		dbInit.ArraySize = NumCascades;
		dbInit.InitialState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		dbInit.Name = L"Sun Shadow Map";
		sunShadowMap.Initialize(dbInit);
	}
}

void MeshRenderer::Initialize(const Model* model_)
{
	model = model_;

	LoadShaders();

	CreateShadowMaps();

	// Create a structured buffer containing texture indices per-material
	{		
		const uint64 numMaterialTextures = model->MaterialTextures().Count();
		const Array<MeshMaterial>& materials = model->Materials();
		const uint64 numMaterials = materials.Size();
		Array<MaterialTextureIndices> textureIndices(numMaterials);
		for (uint64 i = 0; i < numMaterials; ++i)
		{
			MaterialTextureIndices& matIndices = textureIndices[i];
			const MeshMaterial& material = materials[i];

			matIndices.Albedo = material.Textures[uint64(MaterialTextureType::Albedo)]->SRV;
			matIndices.Normal = material.Textures[uint64(MaterialTextureType::Normal)]->SRV;
			matIndices.Roughness = material.Textures[uint64(MaterialTextureType::Roughness)]->SRV;
			matIndices.Metallic = material.Textures[uint64(MaterialTextureType::Metallic)]->SRV;
		}
		
		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(MaterialTextureIndices);
		sbInit.NumElements = numMaterials;
		sbInit.Dynamic = false;
		sbInit.InitData = textureIndices.Data();
		sbInit.Name = L"Material Texture Indices";
		materialTextureIndices.Initialize(sbInit);
	}

	// Main pass root signature
	{
		D3D12_ROOT_PARAMETER1 rootParameters[NumMainPassRootParams] = { };

		// "Standard" descriptor table
		rootParameters[MainPass_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[MainPass_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[MainPass_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		// VSCBuffer
		rootParameters[MainPass_VSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[MainPass_VSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[MainPass_VSCBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[MainPass_VSCBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[MainPass_VSCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		// PSCBuffer
		rootParameters[MainPass_PSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[MainPass_PSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_PSCBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[MainPass_PSCBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[MainPass_PSCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		// ShadowCBuffer
		rootParameters[MainPass_ShadowCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[MainPass_ShadowCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_ShadowCBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[MainPass_ShadowCBuffer].Descriptor.ShaderRegister = 1;
		rootParameters[MainPass_ShadowCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		// MatIndexCBuffer
		rootParameters[MainPass_MatIndexCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[MainPass_MatIndexCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_MatIndexCBuffer].Constants.Num32BitValues = 1;
		rootParameters[MainPass_MatIndexCBuffer].Constants.RegisterSpace = 0;
		rootParameters[MainPass_MatIndexCBuffer].Constants.ShaderRegister = 2;

		// SRV descriptor indices
		rootParameters[MainPass_SRVIndices].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[MainPass_SRVIndices].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_SRVIndices].Descriptor.RegisterSpace = 0;
		rootParameters[MainPass_SRVIndices].Descriptor.ShaderRegister = 3;
		rootParameters[MainPass_SRVIndices].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[3] = {};
		staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Anisotropic, 0);
		staticSamplers[1] = DX12::GetStaticSamplerState(SamplerState::Linear, 1);
		staticSamplers[2] = DX12::GetStaticSamplerState(SamplerState::ShadowMapPCF, 2);
		
		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
		rootSignatureDesc.pStaticSamplers = staticSamplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		DX12::CreateRootSignature(&mainPassRootSignature, rootSignatureDesc);
	}

	// Depth root signature
	{
		D3D12_ROOT_PARAMETER1 rootParameters[1] = { };

		// VSCBuffer
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.RegisterSpace = 0;
		rootParameters[0].Descriptor.ShaderRegister = 0;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		DX12::CreateRootSignature(&depthRootSignature, rootSignatureDesc);
	}
}

void MeshRenderer::Shutdown()
{
	DestroyPSOs();
	sunShadowMap.Shutdown();
	sunFilterableShadowMap.Shutdown();
	tempFilterableShadowMap.Shutdown();
	materialTextureIndices.Shutdown();
	DX12::Release(mainPassRootSignature);
	DX12::Release(depthRootSignature);
}

void MeshRenderer::CreatePSOs(DXGI_FORMAT mainRTFormat, DXGI_FORMAT depthFormat, uint32 numMSAASamples)
{
	if (model == nullptr)
		return;

	ID3D12Device* device = DX12::Device;

	{
		// Main pass PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mainPassRootSignature;
		psoDesc.VS = meshVS.ByteCode();
		psoDesc.PS = meshPSForward.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::WritesEnabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = mainRTFormat;
		psoDesc.DSVFormat = depthFormat;
		psoDesc.SampleDesc.Count = numMSAASamples;
		psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
		psoDesc.InputLayout.NumElements = uint32(Model::NumInputElements());
		psoDesc.InputLayout.pInputElementDescs = Model::InputElements();
		DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mainPassPSO)));
	}

	{
		// Sun shadow depth PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = depthRootSignature;
		psoDesc.VS = meshVS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::WritesEnabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 0;
		psoDesc.DSVFormat = depthFormat;
		psoDesc.SampleDesc.Count = numMSAASamples;
		psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
		psoDesc.InputLayout.NumElements = uint32(Model::NumInputElements());
		psoDesc.InputLayout.pInputElementDescs = Model::InputElements();
		DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthPSO)));

		// Sun shadow depth PSO
		psoDesc.DSVFormat = sunShadowMap.DSVFormat;
		psoDesc.SampleDesc.Count = sunShadowMap.MSAASamples;
		psoDesc.SampleDesc.Quality = sunShadowMap.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCullNoZClip);
		DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sunShadowPSO)));
	}
}

void MeshRenderer::DestroyPSOs()
{
	DX12::DeferredRelease(mainPassPSO);
	DX12::DeferredRelease(depthPSO);
	DX12::DeferredRelease(sunShadowPSO);
}

// Renders all meshes in the model, with shadows
void MeshRenderer::RenderMainPass(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const MainPassData& mainPassData)
{
	PIXMarker marker(cmdList, "Mesh Rendering");

	cmdList->SetGraphicsRootSignature(mainPassRootSignature);
	cmdList->SetPipelineState(mainPassPSO);

	DX12::BindStandardDescriptorTable(cmdList, MainPass_StandardDescriptors, CmdListMode::Graphics);

	Float4x4 world;
	MeshVSConstants vsConstants;
	vsConstants.World = world;
	vsConstants.View = camera.ViewMatrix();
	vsConstants.WorldViewProjection = world * camera.ViewProjectionMatrix();
	DX12::BindTempConstantBuffer(cmdList, vsConstants, MainPass_VSCBuffer, CmdListMode::Graphics);

	ShadingConstants psConstants;
	psConstants.SunDirectionWS = AppSettings::SunDirection; //Float3(0.0f, 0.0f, -1.0f);
	psConstants.SunIrradiance = mainPassData.SkyCache->SunIrradiance; //Float3(1.0f, 1.0f, 1.0f);
	psConstants.CosSunAngularRadius = std::cos(DegToRad(AppSettings::SunSize));
	psConstants.SinSunAngularRadius = std::sin(DegToRad(AppSettings::SunSize));
	psConstants.CameraPosWS = camera.Position();

	psConstants.EnvSH = mainPassData.EnvSH;
	DX12::BindTempConstantBuffer(cmdList, psConstants, MainPass_PSCBuffer, CmdListMode::Graphics);

	if (AppSettings::UseFilterableShadows())
		sunShadowConstants.Extra = Float4(AppSettings::PositiveExponent, AppSettings::NegativeExponent,
			AppSettings::LightBleedingReduction, AppSettings::EVSMBias);
	
	DX12::BindTempConstantBuffer(cmdList, sunShadowConstants, MainPass_ShadowCBuffer, CmdListMode::Graphics);

	SRVIndexConstants SRVIndexConstants;
	SRVIndexConstants.sunShadowMapIdx = sunShadowMap.SRV();
	SRVIndexConstants.MaterialTextureIndicesIdx = materialTextureIndices.SRV;
	SRVIndexConstants.SpecularCubemapLookupIdx = mainPassData.SpecularLUT->SRV;
	SRVIndexConstants.SpecularCubemapIdx = mainPassData.SpecularCubMap->SRV;

	if (AppSettings::UseFilterableShadows())
		SRVIndexConstants.sunShadowMapIdx = sunFilterableShadowMap.SRV();

	DX12::BindTempConstantBuffer(cmdList, SRVIndexConstants, MainPass_SRVIndices, CmdListMode::Graphics);

	// Bind vertices and indices
	D3D12_VERTEX_BUFFER_VIEW vbView = model->VertexBuffer().VBView();
	D3D12_INDEX_BUFFER_VIEW ibView = model->IndexBuffer().IBView();

	cmdList->IASetVertexBuffers(0, 1, &vbView);
	cmdList->IASetIndexBuffer(&ibView);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw all visible meshes
	uint32 currMaterial = uint32(-1);
	uint64 numMeshes = model->NumMeshes();

	for (uint64 i = 0; i < numMeshes; ++i)
	{
		const Mesh& mesh = model->Meshes()[i];

		// Draw all parts
		for (uint64 partIdx = 0; partIdx < mesh.NumMeshParts(); ++partIdx)
		{
			const MeshPart& part = mesh.MeshParts()[partIdx];
			if (part.MaterialIdx != currMaterial)
			{
				const MeshMaterial& material = model->Materials()[part.MaterialIdx];
				cmdList->SetGraphicsRoot32BitConstant(MainPass_MatIndexCBuffer, part.MaterialIdx, 0);
				currMaterial = part.MaterialIdx;
			}
			cmdList->DrawIndexedInstanced(part.IndexCount, 1, mesh.IndexOffset() + part.IndexStart, mesh.VertexOffset(), 0);
		}
	}
}

void MeshRenderer::RenderDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera, ID3D12PipelineState* pso)
{
	cmdList->SetGraphicsRootSignature(depthRootSignature);
	cmdList->SetPipelineState(pso);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Float4x4 world;
	MeshVSConstants vsConstants;
	vsConstants.World = world;
	vsConstants.View = camera.ViewMatrix();
	vsConstants.WorldViewProjection = world * camera.ViewProjectionMatrix();
	DX12::BindTempConstantBuffer(cmdList, vsConstants, 0, CmdListMode::Graphics);

	// Bind vertices and indices
	D3D12_VERTEX_BUFFER_VIEW vbView = model->VertexBuffer().VBView();
	D3D12_INDEX_BUFFER_VIEW ibView = model->IndexBuffer().IBView();
	cmdList->IASetVertexBuffers(0, 1, &vbView);
	cmdList->IASetIndexBuffer(&ibView);

	// Draw all meshes
	uint64 numMeshes = model->NumMeshes();
	for (uint64 i = 0; i < numMeshes; ++i)
	{
		const Mesh& mesh = model->Meshes()[i];
		cmdList->DrawIndexedInstanced(mesh.NumIndices(), 1, mesh.IndexOffset(), mesh.VertexOffset(), 0);
	}
}

// Renders all meshes using depth-only rendering for a sun shadow map
void MeshRenderer::RenderSunShadowDepth(ID3D12GraphicsCommandList2* cmdList, const Camera& camera)
{
	//TODO: Frastum Culling
	RenderDepth(cmdList, camera, sunShadowPSO);
}

// Renders meshes using cascaded shadow mapping
void MeshRenderer::RenderSunShadowMap(ID3D12GraphicsCommandList2* cmdList, const Camera& camera)
{
	if (AppSettings::UseFilterableShadows())
		RenderSunFilterableShadowMap(cmdList, camera);
	else
		RenderSunDepthShadowMap(cmdList, camera);
}

void MeshRenderer::RenderSunDepthShadowMap(ID3D12GraphicsCommandList2* cmdList, const Camera& camera)
{
	PIXMarker marker(cmdList, L"Sun Shadow Map Rendering");

	OrthographicCamera cascadeCameras[NumCascades];
	ShadowHelper::PrepareCascades(AppSettings::SunDirection, SunShadowMapSize, true, camera, sunShadowConstants.Base, cascadeCameras);

	// Transition all of the cascade array slices to a writable state
	sunShadowMap.MakeWritable(cmdList);

	for (uint64 cascadeIdx = 0; cascadeIdx < NumCascades; ++cascadeIdx)
	{
		PIXMarker cascadeMarker(cmdList, MakeString(L"Rendering Shadow Map Cascade %u", cascadeIdx).c_str());

		DX12::SetViewport(cmdList, SunShadowMapSize, SunShadowMapSize);

		D3D12_CPU_DESCRIPTOR_HANDLE dsv = sunShadowMap.ArrayDSVs[cascadeIdx];

		cmdList->OMSetRenderTargets(0, nullptr, false, &dsv);
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		RenderSunShadowDepth(cmdList, cascadeCameras[cascadeIdx]);
	}

	sunShadowMap.MakeReadable(cmdList);
}

void MeshRenderer::RenderSunFilterableShadowMap(ID3D12GraphicsCommandList2* cmdList, const Camera& camera)
{
	PIXMarker marker(cmdList, L"Sun Shadow Map Rendering");

	OrthographicCamera cascadeCameras[NumCascades];
	ShadowHelper::PrepareCascades(AppSettings::SunDirection, SunShadowMapSize, true, camera, sunShadowConstants.Base, cascadeCameras);

	// Transition all of the cascade array slices to a writable state
	sunFilterableShadowMap.MakeWritable(cmdList);

	for (uint64 cascadeIdx = 0; cascadeIdx < NumCascades; ++cascadeIdx)
	{
		PIXMarker cascadeMarker(cmdList, MakeString(L"Rendering Shadow Map Cascade %u", cascadeIdx).c_str());

		DX12::SetViewport(cmdList, SunShadowMapSize, SunShadowMapSize);

		D3D12_CPU_DESCRIPTOR_HANDLE dsv = sunShadowMap.DSV;

		cmdList->OMSetRenderTargets(0, nullptr, false, &dsv);
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		RenderSunShadowDepth(cmdList, cascadeCameras[cascadeIdx]);

		sunShadowMap.MakeReadable(cmdList);

		ShadowHelper::ConvertShadowMap(cmdList, sunShadowMap, sunFilterableShadowMap, (uint32)cascadeIdx,
			tempFilterableShadowMap, 5, 5, false, AppSettings::PositiveExponent, AppSettings::NegativeExponent);

		sunShadowMap.MakeWritable(cmdList);
	}

	//sunFilterableShadowMap.MakeReadable(cmdList);

	DX12::GenerateMips(cmdList, sunFilterableShadowMap);
}

}
