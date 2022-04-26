#include "PCH.h"

#include "Graphics\\DX12.h"
#include "Graphics\\DX12_Helpers.h"
#include "Graphics\\DX12_Upload.h"
#include "Graphics\\ShaderCompilation.h"

#include "MyD3DProject.h"

using namespace Framework;

struct ColorConstants
{
	XMFLOAT4X4 WorldViewProj;
};

enum ColorRootParams : uint32
{
	ColorParams_CBuffer,

	NumColorRootParams,
};

MyD3DProject::MyD3DProject(const wchar* cmdLine) : App(L"MyD3D", cmdLine)
{
}

void MyD3DProject::BeforeReset()
{
}

void MyD3DProject::AfterReset()
{
	CreateRenderTargets();
}

void MyD3DProject::CreateRenderTargets()
{
	uint32 width = swapChain.Width();
	uint32 height = swapChain.Height();
	const uint32 NumSamples = 1;

	{
		DepthBufferInit dbInit;
		dbInit.Width = width;
		dbInit.Height = height;
		dbInit.Format = DXGI_FORMAT_D32_FLOAT;
		dbInit.MSAASamples = NumSamples;
		dbInit.InitialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
		dbInit.Name = L"Main Depth Buffer";
		depthBuffer.Initialize(dbInit);
	}

	{
		// Transition depth buffers to a writable state
		D3D12_RESOURCE_BARRIER barriers[1] = {};
		
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = depthBuffer.Resource();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[0].Transition.Subresource = 0;

		DX12::CmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}
}

void MyD3DProject::Initialize()
{
	DX12::FlushGPU();
	boxModel.GenerateBox();
	
	{
		CompileOptions opts;
		opts.Add("TestMacro_", 1);

		// shaders
		colorVS = CompileFromFile(L"Shaders\\color.hlsl", "VS", ShaderType::Vertex, opts);
		colorPS = CompileFromFile(L"Shaders\\color.hlsl", "PS", ShaderType::Pixel, opts);
	}

	{
		// root signature
		D3D12_ROOT_PARAMETER1 rootParameters[NumColorRootParams] = {};

		rootParameters[ColorParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[ColorParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[ColorParams_CBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[ColorParams_CBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[ColorParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = { };
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		DX12::CreateRootSignature(&colorRootSignature, rootSignatureDesc);
	}
}

void MyD3DProject::Shutdown()
{
	boxModel.Shutdown();

	DX12::Release(colorRootSignature);	

	depthBuffer.Shutdown();
}

void MyD3DProject::CreatePSOs()
{
	ID3D12Device* device = DX12::Device;

	const RenderTexture& mainTarget = swapChain.BackBuffer();

	{
		// PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = colorRootSignature;
		psoDesc.VS = colorVS.ByteCode();
		psoDesc.PS = colorPS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::WritesEnabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = mainTarget.Texture.Format;
		psoDesc.DSVFormat = depthBuffer.DSVFormat;
		psoDesc.SampleDesc.Count = mainTarget.MSAASamples;
		psoDesc.SampleDesc.Quality = mainTarget.MSAAQuality;
		psoDesc.InputLayout.NumElements = uint32(Model::NumInputElements());
		psoDesc.InputLayout.pInputElementDescs = Model::InputElements();
		DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&colorPSO)));
	}
}

void MyD3DProject::DestroyPSOs()
{
	DX12::DeferredRelease(colorPSO);
}

void MyD3DProject::Update(const Timer& timer)
{
}

void MyD3DProject::Render(const Timer& timer)
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;
	
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
	cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(depthBuffer.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	DX12::SetViewport(cmdList, swapChain.Width(), swapChain.Height());

	cmdList->SetGraphicsRootSignature(colorRootSignature);
	cmdList->SetPipelineState(colorPSO);
	
	ColorConstants colorConstants;	
	colorConstants.WorldViewProj = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	DX12::BindTempConstantBuffer(cmdList, colorConstants, ColorParams_CBuffer, CmdListMode::Graphics);

	const Model* curModel = &boxModel;

	// Bind vertices and indices
	D3D12_VERTEX_BUFFER_VIEW vbView = curModel->VertexBuffer().VBView();
	D3D12_INDEX_BUFFER_VIEW ibView = curModel->IndexBuffer().IBView();

	cmdList->IASetVertexBuffers(0, 1, &vbView);
	cmdList->IASetIndexBuffer(&ibView);
	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	uint64 numMeshes = curModel->NumMeshes();
	for (uint64 i = 0; i < numMeshes; ++i)
	{
		const Mesh& mesh = curModel->Meshes()[i];

		// Draw all parts
		for (uint64 partIdx = 0; partIdx < mesh.NumMeshParts(); ++partIdx)
		{
			const MeshPart& part = mesh.MeshParts()[partIdx];
			cmdList->DrawIndexedInstanced(part.IndexCount, 1, mesh.IndexOffset() + part.IndexStart, mesh.VertexOffset(), 0);
		}
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	//FILE* fp;

	//AllocConsole();
	//freopen_s(&fp, "CONIN$", "r", stdin);
	//freopen_s(&fp, "CONOUT$", "w", stdout);
	//freopen_s(&fp, "CONOUT$", "w", stderr);

	MyD3DProject app(lpCmdLine);
	app.Run();
	return 0;
}