#include "MeshRenderer.h"

#include "..\\Exceptions.h"
#include "..\\Utility.h"

namespace Framework
{

enum MainPassRootParams : uint32
{	
	MainPass_StandardDescriptors,
	MainPass_VSCBuffer,
	MainPass_PSCBuffer,
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
	uint32 MaterialTextureIndicesIdx;
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
	CompileOptions opts;
	meshVS = CompileFromFile(L"Shaders\\Mesh.hlsl", "VS", ShaderType::Vertex, opts);
	meshPSForward = CompileFromFile(L"Shaders\\Mesh.hlsl", "PSForward", ShaderType::Pixel, opts);
}

void MeshRenderer::Initialize(const Model* model_)
{
	model = model_;

	LoadShaders();

	{
		// Create a structured buffer containing texture indices per-material
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

	{
		// Main pass root signature
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

		// MatIndexCBuffer
		rootParameters[MainPass_MatIndexCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[MainPass_MatIndexCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_MatIndexCBuffer].Constants.Num32BitValues = 1;
		rootParameters[MainPass_MatIndexCBuffer].Constants.RegisterSpace = 0;
		rootParameters[MainPass_MatIndexCBuffer].Constants.ShaderRegister = 1;

		// SRV descriptor indices
		rootParameters[MainPass_SRVIndices].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[MainPass_SRVIndices].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[MainPass_SRVIndices].Descriptor.RegisterSpace = 0;
		rootParameters[MainPass_SRVIndices].Descriptor.ShaderRegister = 2;
		rootParameters[MainPass_SRVIndices].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Anisotropic, 0);
		
		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
		rootSignatureDesc.pStaticSamplers = staticSamplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		DX12::CreateRootSignature(&mainPassRootSignature, rootSignatureDesc);
	}
}

void MeshRenderer::Shutdown()
{
	DestroyPSOs();
	materialTextureIndices.Shutdown();
	DX12::Release(mainPassRootSignature);
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
}

void MeshRenderer::DestroyPSOs()
{
	DX12::DeferredRelease(mainPassPSO);
}

// Renders all meshes in the model, with shadows
void MeshRenderer::RenderMainPass(ID3D12GraphicsCommandList* cmdList, const Camera& camera)
{
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
	psConstants.SunDirectionWS = Float3::Normalize(Float3(1.0f, 1.0f, -1.0f));
	psConstants.SunIrradiance = Float3(1.0f, 1.0f, 1.0f);
	psConstants.CameraPosWS = camera.Position();
	DX12::BindTempConstantBuffer(cmdList, psConstants, MainPass_PSCBuffer, CmdListMode::Graphics);

	SRVIndexConstants SRVIndexConstants;
	SRVIndexConstants.MaterialTextureIndicesIdx = materialTextureIndices.SRV;
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

}
