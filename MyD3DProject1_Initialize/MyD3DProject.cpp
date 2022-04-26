#include "PCH.h"

#include "MyD3DProject.h"

using namespace Framework;

MyD3DProject::MyD3DProject(const wchar* cmdLine) : App(L"MyD3D", cmdLine)
{
}

void MyD3DProject::Initialize()
{
	// Check if the device supports conservative rasterization
}

void MyD3DProject::Shutdown()
{
}

void MyD3DProject::Update(const Timer& timer)
{
}

void MyD3DProject::Render(const Timer& timer)
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
	cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);

	DX12::SetViewport(cmdList, swapChain.Width(), swapChain.Height());
}

void MyD3DProject::BeforeReset()
{
}

void MyD3DProject::AfterReset()
{
}

void MyD3DProject::CreatePSOs()
{
}

void MyD3DProject::DestroyPSOs()
{
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MyD3DProject app(lpCmdLine);
	app.Run();
	return 0;
}