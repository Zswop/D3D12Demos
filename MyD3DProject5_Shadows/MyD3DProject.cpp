#include "PCH.h"

#include "Graphics\\DX12.h"
#include "Graphics\\DX12_Helpers.h"
#include "Graphics\\DX12_Upload.h"
#include "Graphics\\ShaderCompilation.h"
#include "Graphics\\Textures.h"
#include "Graphics\\Sampling.h"
#include "Graphics\\BRDF.h"
#include "Graphics\\ShadowHelper.h"

#include "FileIO.h"
#include "Input.h"

#include "AppSettings.h"
#include "MyD3DProject.h"

using namespace Framework;

static void GenerateEnvSpecularLookupTexture(Texture& texture)
{
	std::string csvOutput = "SqrtRoughness,NDotV,A,B,Delta\n";

	const uint32 NumVSamples = 64;
	const uint32 NumRSamples = 64;
	const uint32 SqrtNumSamples = 32;
	const uint32 NumSamples = SqrtNumSamples * SqrtNumSamples;

	TextureData<Half4> textureData;
	textureData.Init(NumVSamples, NumRSamples, 1);

	uint32 texelIdx = 0;

	const Float3 n = Float3(0.0f, 0.0f, 1.0f);

	for (uint32 rIdx = 0; rIdx < NumRSamples; ++rIdx)
	{
		const float sqrtRoughness = (rIdx + 0.5f) / NumRSamples;
		const float roughness = Max(sqrtRoughness * sqrtRoughness, 0.001f);

		for (uint32 vIdx = 0; vIdx < NumVSamples; ++vIdx)
		{
			const float nDotV = (vIdx + 0.5f) / NumVSamples;

			Float3 v = 0.0f;
			v.z = nDotV;
			v.x = std::sqrt(1.0f - Saturate(v.z * v.z));			

			float A = 0.0f;
			float B = 0.0f;

			for (uint32 sIdx = 0; sIdx < NumSamples; ++sIdx)
			{
				const Float2 u1u2 = SampleCMJ2D(sIdx, SqrtNumSamples, SqrtNumSamples, 0);

				const Float3 h = SampleGGXMicrofacet(roughness, u1u2.x, u1u2.y);
				const float hDotV = Float3::Dot(h, v);
				const Float3 l = 2.0f * hDotV * h - v;

				const float nDotL = l.z;

				if (nDotL > 0.0f)
				{
					const float pdf = SampleDirectionGGX_PDF(n, h, v, roughness);
					const float sampleWeight = GGX_Specular(roughness, n, h, v, l) * nDotL / pdf;

					const float fc = std::pow(1 - Saturate(hDotV), 5.0f);

					A += (1.0f - fc) * sampleWeight;
					B += fc * sampleWeight;
				}
			}

			A /= NumSamples;
			B /= NumSamples;
			textureData.Texels[texelIdx] = Half4(A, B, 0, 1);

			++texelIdx;

			csvOutput += MakeString("%f,%f,%f,%f,%f\n", sqrtRoughness, nDotV, A, B, A + B);
		}
	}

	//SaveTextureAsDDS(textureData, L"Content\\EnvSpecularLUT.dds");
	Create2DTexture(texture, textureData.Width, textureData.Height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, false, textureData.Texels.Data());
	SaveTextureAsDDS(texture, L"Content\\EnvSpecularLUT.dds");
}

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
		RenderTextureInit rtInit;
		rtInit.Width = width;
		rtInit.Height = height;
		rtInit.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtInit.MSAASamples = NumSamples;
		rtInit.ArraySize = 1;
		rtInit.CreateUAV = NumSamples == 1;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Main Target";
		mainTarget.Initialize(rtInit);
	}

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
	
}

void MyD3DProject::Initialize()
{
	ShadowHelper::Initialize();

	float aspect = float(swapChain.Width()) / swapChain.Height();
	camera.Initialize(aspect, Pi_4, 0.1f, 100.0f);

	//sceneModel.GenerateBox(Float3(), Float3(1.0f, 1.0f, 1.0f), Quaternion(),
	//	L"Sponza_Bricks_a_Albedo.png", L"Sponza_Bricks_a_Normal.png", L"Sponza_Bricks_a_Roughness.png");

	sceneModel.GenerateTestScene();
	currentModel = &sceneModel;

	meshRenderer.Shutdown();
	DX12::FlushGPU();
	meshRenderer.Initialize(currentModel);

	camera.SetPosition(Float3(0.0f, 3.2f, -3.0f));
	camera.SetXRotation(Framework::DegToRad(15));
	//camera.SetYRotation(Framework::DegToRad(30));

	skybox.Initialize();

	postProcessor.Initialize();

	uint64 numCubMaps = AppSettings::NumCubeMaps;
	envMaps.Init(numCubMaps);
	for (uint64 i = 0; i < AppSettings::NumCubeMaps; ++i)
	{
		LoadTexture(envMaps[i].EnvMap, AppSettings::CubeMapPaths(i), false);
		envMaps[i].projectToSH = false;

		//envMaps[i].SH = ProjectCubemapToSH(envMaps[i].EnvMap);
		//CreateCubemapFromSH9Color(envMaps[i].RadianceMap, envMaps[i].SH);
		//CreateTexture2DFromSH9Color(envMaps[i].SH, L"Content\\IrradianceMap.dds");

		/*TextureData<Half4> textureData;
		GetTextureData(envMaps[i].EnvMap, textureData);

		Create2DTexture(envMaps[i].RadianceMap, textureData.Width, textureData.Height, 1, 1,
			DXGI_FORMAT_R16G16B16A16_FLOAT, true, textureData.Texels.Data());*/
	}

	//GenerateEnvSpecularLookupTexture(envSpecularLUT);
}

void MyD3DProject::Shutdown()
{
	ShadowHelper::Shutdown();

	sceneModel.Shutdown();
	meshRenderer.Shutdown();

	envSpecularLUT.Shutdown();

	for (uint64 i = 0; i < envMaps.Size(); ++i)	
		envMaps[i].EnvMap.Shutdown();	
	envMaps.Shutdown();
	
	skybox.Shutdown();
	skyCache.Shutdown();
	postProcessor.Shutdown();

	mainTarget.Shutdown();
	depthBuffer.Shutdown();
}

void MyD3DProject::CreatePSOs()
{
	ShadowHelper::CreatePSOs();
	meshRenderer.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
	skybox.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
}

void MyD3DProject::DestroyPSOs()
{
	ShadowHelper::DestroyPSOs();
	meshRenderer.DestroyPSOs();
	skybox.DestoryPSOs();
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

	skyCache.Init(AppSettings::SunDirection, AppSettings::SunSize, AppSettings::GroundAlbedo, AppSettings::Turbidity, true);
	envSH = skyCache.SH;

	if ((uint32)AppSettings::SkyMode >= AppSettings::CubeMapStart)
	{
		EnvironmentMap& envMap = envMaps[(uint32)AppSettings::SkyMode - AppSettings::CubeMapStart];
		if (envMap.projectToSH == false)
		{
			envMap.SH = ProjectCubemapToSH(envMap.EnvMap);
			envMap.projectToSH = true;
		}
		envSH = envMap.SH;
	}

	// Toggle VSYNC	
	if (kbState.IsKeyDown(KeyboardState::V))
		swapChain.SetVSYNCEnabled(swapChain.VSYNEnabled() ? false : true);
}

void MyD3DProject::Render(const Timer& timer)
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	meshRenderer.RenderSunShadowMap(cmdList, camera);

	RenderForward();

	postProcessor.Render(cmdList, mainTarget, swapChain.BackBuffer());

	//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
	//cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);
}

void MyD3DProject::RenderForward()
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	PIXMarker marker(cmdList, "Forward rendering");

	{
		// Transition depth buffers to a writable state
		D3D12_RESOURCE_BARRIER barriers[3] = {};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = mainTarget.Resource();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.Subresource = 0;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = depthBuffer.Resource();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[1].Transition.Subresource = 0;

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[2].Transition.pResource = meshRenderer.SunShadowMap().Resource();
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { mainTarget.RTV };
	cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(depthBuffer.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	DX12::SetViewport(cmdList, mainTarget.Width(), mainTarget.Height());

	// Render the main forward pass
	MainPassData mainPassData;
	mainPassData.SkyCache = &skyCache;
	mainPassData.EnvSH = envSH;
	meshRenderer.RenderMainPass(cmdList, camera, mainPassData);

	// Render the sky
	if ((uint32)AppSettings::SkyMode >= AppSettings::CubeMapStart)
	{
		EnvironmentMap& envMap = envMaps[(uint32)AppSettings::SkyMode - AppSettings::CubeMapStart];
		skybox.RenderEnvironmentMap(cmdList, camera.ViewMatrix(), camera.ProjectionMatrix(), &envMap.EnvMap);
	}
	else
	{
		skybox.RenderSky(cmdList, camera.ViewMatrix(), camera.ProjectionMatrix(), skyCache, true);
	}
	

	{
		// Transition depth buffers to a writable state
		D3D12_RESOURCE_BARRIER barriers[3] = {};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = mainTarget.Resource();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[0].Transition.Subresource = 0;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = depthBuffer.Resource();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
		barriers[1].Transition.Subresource = 0;

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[2].Transition.pResource = meshRenderer.SunShadowMap().Resource();
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
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