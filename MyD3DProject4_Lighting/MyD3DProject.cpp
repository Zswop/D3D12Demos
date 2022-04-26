#include "PCH.h"

#include "Graphics\\DX12.h"
#include "Graphics\\DX12_Helpers.h"
#include "Graphics\\DX12_Upload.h"
#include "Graphics\\ShaderCompilation.h"

#include "Input.h"

#include "MyD3DProject.h"

using namespace Framework;

MyD3DProject::MyD3DProject(const wchar* cmdLine) : App(L"MyD3D", cmdLine)
{
}

void MyD3DProject::BeforeReset()
{
}

void MyD3DProject::AfterReset()
{
	float aspect = float(swapChain.Width()) / swapChain.Height();
	camera.SetAspectRatio(aspect);

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
	float aspect = float(swapChain.Width()) / swapChain.Height();
	camera.Initialize(aspect, Pi_4, 0.1f, 35.0f);

	//sceneModel.GenerateBox(Float3(), Float3(1.0f, 1.0f, 1.0f), Quaternion(),
	//	L"Sponza_Bricks_a_Albedo.png", L"Sponza_Bricks_a_Normal.png", L"Sponza_Bricks_a_Roughness.png");

	sceneModel.GenerateTestScene();
	currentModel = &sceneModel;

	meshRenderer.Shutdown();
	DX12::FlushGPU();
	meshRenderer.Initialize(currentModel);

	camera.SetPosition(Float3(0.0f, 3.0f, -8.0f));
	camera.SetXRotation(Framework::DegToRad(20));
	//camera.SetYRotation(Framework::DegToRad(30));
}

void MyD3DProject::Shutdown()
{
	sceneModel.Shutdown();
	meshRenderer.Shutdown();

	depthBuffer.Shutdown();
}

void MyD3DProject::CreatePSOs()
{
	const RenderTexture& mainTarget = swapChain.BackBuffer();
	meshRenderer.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
}

void MyD3DProject::DestroyPSOs()
{
	meshRenderer.DestroyPSOs();
}

void MyD3DProject::Update(const Timer& timer)
{
	MouseState mouseState = MouseState::GetMouseState(window);
	KeyboardState kbState = KeyboardState::GetKeyboardState(window);

	if (kbState.IsKeyDown(KeyboardState::Escape))
		window.Destroy();

	float CamMoveSpeed = 5.0f * timer.DeltaSecondsF();
	const float CamRotSpeed = 0.180f * timer.DeltaSecondsF();

	if (kbState.IsKeyDown(KeyboardState::LeftShift))
		CamMoveSpeed *= 0.25f;

	Float3 camPos = camera.Position();
	if (kbState.IsKeyDown(KeyboardState::W))
		camPos += camera.Forward() * CamMoveSpeed;
	else if (kbState.IsKeyDown(KeyboardState::S))
		camPos += camera.Back() * CamMoveSpeed;
	else if (kbState.IsKeyDown(KeyboardState::A))
		camPos += camera.Left() * CamMoveSpeed;
	else if (kbState.IsKeyDown(KeyboardState::D))
		camPos += camera.Right() * CamMoveSpeed;

	if (kbState.IsKeyDown(KeyboardState::Q))
		camPos += camera.Up() * CamMoveSpeed;
	else if (kbState.IsKeyDown(KeyboardState::E))
		camPos += camera.Down() * CamMoveSpeed;

	camera.SetPosition(camPos);

	// Rotate the camera with the mouse
	if (mouseState.RButton.Pressed && mouseState.IsOverWindow)
	{
		float xRot = camera.XRotation();
		float yRot = camera.YRotation();
		xRot += mouseState.DY * CamRotSpeed;
		yRot += mouseState.DX * CamRotSpeed;
		camera.SetXRotation(xRot);
		camera.SetYRotation(yRot);
	}

	// Toggle VSYNC	
	if (kbState.IsKeyDown(KeyboardState::V))
		swapChain.SetVSYNCEnabled(swapChain.VSYNEnabled() ? false : true);
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

	meshRenderer.RenderMainPass(cmdList, camera);
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