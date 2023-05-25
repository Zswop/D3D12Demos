#include "PCH.h"

#include "App.h"
#include "Exceptions.h"
#include "Graphics\\ShaderCompilation.h"
#include "Graphics\\Spectrum.h"
#include "ImGuiHelper.h"

// AppSettings framework
namespace AppSettings
{
	void Initialize();
	void Shutdown();
	void Update(uint32 displayWidth, uint32 displayHeight);
	void UpdateCBuffer();
}

namespace Framework
{

App* GlobalApp = nullptr;

App::App(const wchar* appName, const wchar* cmdLine) :
	window(nullptr, appName, WS_OVERLAPPEDWINDOW,
		WS_EX_APPWINDOW, 1280, 720), applicationName(appName)
{
	GlobalApp = this;

	SampledSpectrum::Init();

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
	
	if (DX12::CurrentCPUFrame % 60 == 0)
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

	// Initialize ImGui
	ImGuiHelper::Initialize(window);

	AppSettings::Initialize();

	Initialize();
}

void App::Shutdown_Internal()
{
	DX12::FlushGPU();	
	DestroyPSOs();
	ImGuiHelper::Shutdown();
	ShutdownShaders();
	swapChain.Shutdown();

	AppSettings::Shutdown();

	Shutdown();

	DX12::Shutdown();
}

void App::Update_Internal()
{
	appTimer.Update();

	const uint32 displayWidth = swapChain.Width();
	const uint32 displayHeight = swapChain.Height();
	ImGuiHelper::BeginFrame(displayWidth, displayHeight, appTimer.DeltaSecondsF());

	CalculateFPS();

	AppSettings::Update(displayWidth, displayHeight);

	Update(appTimer);
}

void App::Render_Internal()
{
	if (UpdateShaders())
	{
		DestroyPSOs();
		CreatePSOs();
	}

	AppSettings::UpdateCBuffer();

	DX12::BeginFrame();
	swapChain.BeginFrame();

	Render(appTimer);

	const uint32 displayWidth = swapChain.Width();
	const uint32 displayHeight = swapChain.Height();

	DrawLog();

	ImGuiHelper::EndFrame(DX12::CmdList, swapChain.BackBuffer().RTV, displayWidth, displayHeight);

	swapChain.EndFrame();

	DX12::EndFrame(swapChain.D3DSwapChain(), swapChain.NumVSYNCIntervals());
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
	ImGuiHelper::CreatePSOs(swapChain.Format());

	CreatePSOs();
}

void App::DestroyPSOs_Internal()
{
	ImGuiHelper::DestroyPSOs();

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

void App::DrawLog()
{
	const uint32 displayWidth = swapChain.Width();
	const uint32 displayHeight = swapChain.Height();

	if (showLog == false)
	{
		ImGui::SetNextWindowSize(ImVec2(75.0f, 25.0f));
		ImGui::SetNextWindowPos(ImVec2(25.0f, displayHeight - 50.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
		if (ImGui::Begin("log_button", nullptr, ImVec2(75.0f, 25.0f), 0.0f, flags))
		{
			if (ImGui::Button("Log"))
				showLog = true;
		}

		ImGui::PopStyleVar();

		ImGui::End();

		return;
	}

	ImVec2 initialSize = ImVec2(displayWidth * 0.5f, float(displayHeight) * 0.25f);
	ImGui::SetNextWindowSize(initialSize, ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(10.0f, displayHeight - initialSize.y - 10.0f), ImGuiSetCond_FirstUseEver);

	if (ImGui::Begin("Log", &showLog) == false)
	{
		ImGui::End();
		return;
	}

	const uint64 numMessages = numLogMessages;
	const uint64 start = numMessages > MaxLogMessages ? numMessages - MaxLogMessages : 0;
	for (uint64 i = start; i < numMessages; ++i)
		ImGui::TextUnformatted(logMessages[i % MaxLogMessages].c_str());

	if (newLogMessage)
		ImGui::SetScrollHere();

	ImGui::End();

	newLogMessage = false;
}

void App::AddToLog(const char* msg)
{
	if (msg == nullptr)
		return;

	const uint64 idx = uint64(InterlockedIncrement64(&numLogMessages) - 1) % MaxLogMessages;
	logMessages[idx] = msg;

	newLogMessage = true;
}

}