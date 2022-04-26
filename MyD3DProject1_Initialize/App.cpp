#include "PCH.h"

#include "App.h"
#include "Exceptions.h"

namespace Framework
{

App* GlobalApp = nullptr;

App::App(const wchar* appName, const wchar* cmdLine) :
	window(nullptr, appName, WS_OVERLAPPEDWINDOW,
		WS_EX_APPWINDOW, 1280, 720), applicationName(appName)
{
	GlobalApp = this;
	ParseCommandLine(cmdLine);
}

App::~App()
{
}

int32 App::Run()
{
	try
	{
		Initialize_Internal();

		AfterReset_Internal();

		CreatePSOs_Internal();

		while (window.IsAlive())
		{
			if (!window.IsMinimized())
			{
				Update_Internal();

				Render_Internal();
			}

			window.MessageLoop();
		}
	}
	catch (Framework::Exception exception)
	{
		exception.ShowErrorMessage();
		return -1;
	}

	Shutdown_Internal();

	return returnCode;
}


void App::Exit()
{
	window.Destroy();
}

void App::ParseCommandLine(const wchar* cmdLine)
{
	OutputDebugString(cmdLine);
}

void App::CalculateFPS()
{
	timeDeltaBuffer[currentTimeDeltaSample] = appTimer.DeltaSecondsF();
	currentTimeDeltaSample = (currentTimeDeltaSample + 1) % NumTimeDeltaSamples;

	float averageDelta = 0;
	for (uint32 i = 0; i < NumTimeDeltaSamples; ++i)
		averageDelta += timeDeltaBuffer[i];
	averageDelta /= NumTimeDeltaSamples;

	fps = static_cast<uint32>(std::floor(1.0f / averageDelta) + 0.5f);
	
	if (appTimer.ElapsedSeconds() % 10 == 0)
	{
		std::wstring windowText = applicationName + MakeString(L" fps: %d", fps);
		window.SetWindowTitle(windowText.c_str());
	}
}

void App::OnWindowResized(void* context, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg != WM_SIZE)
		return;

	App* app = reinterpret_cast<App*>(context);

	if (!app->swapChain.FullScreen() && wParam != SIZE_MINIMIZED)
	{
		int width, height;
		app->window.GetClientArea(width, height);

		if (uint32(width) != app->swapChain.Width() || uint32(height) != app->swapChain.Height())
		{
			app->DestroyPSOs_Internal();

			app->BeforeReset_Internal();

			app->swapChain.SetWidth(width);
			app->swapChain.SetHeight(height);
			app->swapChain.Reset();

			app->AfterReset_Internal();

			app->CreatePSOs_Internal();
		}
	}
}

void App::Initialize_Internal()
{
	DX12::Initialize(minFeatureLevel, adapterIdx);

	window.SetClientArea(swapChain.Width(), swapChain.Height());
	swapChain.Initialize(window);

	if (showWindow)
		window.ShowWindow();

	window.RegisterMessageCallback(OnWindowResized, this);

	Initialize();
}

void App::Shutdown_Internal()
{
	DX12::FlushGPU();	
	DestroyPSOs_Internal();
	swapChain.Shutdown();

	Shutdown();

	DX12::Shutdown();
}

void App::Update_Internal()
{
	appTimer.Update();

	CalculateFPS();

	Update(appTimer);
}

void App::Render_Internal()
{
	DX12::BeginFrame();
	swapChain.BeginFrame();

	Render(appTimer);

	swapChain.EndFrame();

	DX12::EndFrame(swapChain.D3DSwapChain(), swapChain.NumVSYNIntervals());
}

void App::BeforeReset_Internal()
{
	// Need this in order to resize the swap chain
	DX12::FlushGPU();

	BeforeReset();
}

void App::AfterReset_Internal()
{
	AfterReset();
}

void App::CreatePSOs_Internal()
{
	CreatePSOs();
}

void App::DestroyPSOs_Internal()
{
	DestroyPSOs();
}

void App::ToggleFullScreen(bool fullScreen)
{
	if (fullScreen != swapChain.FullScreen())
	{
		DestroyPSOs_Internal();

		BeforeReset_Internal();

		swapChain.SetFullScreen(fullScreen);
		swapChain.Reset();

		AfterReset_Internal();

		CreatePSOs_Internal();
	}
}

}