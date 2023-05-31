#include "PCH.h"

#include "Graphics\\DX12.h"
#include "Graphics\\DX12_Helpers.h"
#include "Graphics\\DX12_Upload.h"
#include "Graphics\\ShaderCompilation.h"
#include "Graphics\\Textures.h"
#include "Graphics\\Sampling.h"
#include "Graphics\\ShadowHelper.h"

#include "FileIO.h"
#include "Input.h"

#include "AppSettings.h"
#include "MyD3DProject.h"

using namespace Framework;

struct LightConstants
{
	SpotLight Lights[MaxSpotLights];
	Float4x4 ShadowMatrices[MaxSpotLights];
};

struct ResolveConstants
{
	uint32 FilterType;
	uint32 SampleRadius;
	Float2 TextureSize;

	float FilterRadius;
	float FilterGaussianSigma;
	float Exposure;
	float ExposureFilterOffset;

	bool32 EnableTemporalAA;
	float TemporalAABlendFactor;
};

enum ResolveRootParams : uint32
{
	ResolveParams_StandardDescriptors,
	ResolveParams_Constants,
	ResolveParams_SRVIndices,

	NumResolveRootParams
};

MyD3DProject::MyD3DProject(const wchar* cmdLine) : App(L"MyD3D", cmdLine)
{
	minFeatureLevel = D3D_FEATURE_LEVEL_11_1;
}

void MyD3DProject::BeforeReset()
{
}

void MyD3DProject::AfterReset()
{
	uint32 width = swapChain.Width();
	uint32 height = swapChain.Height();

	float aspect = float(width) / height;
	camera.SetAspectRatio(aspect);

	CreateRenderTargets();

	lightCluster.CreateClusterBuffer(width, height);
	pathTracer.CreateRenderTarget(width, height);
	postProcessor.AfterReset(width, height);
}

void MyD3DProject::CreateRenderTargets()
{
	uint32 width = swapChain.Width();
	uint32 height = swapChain.Height();
	const uint32 NumSamples = AppSettings::NumMSAASamples();

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
	
	{
		RenderTextureInit rtInit;
		rtInit.Width = width;
		rtInit.Height = height;
		rtInit.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtInit.MSAASamples = 1;
		rtInit.ArraySize = 1;
		rtInit.CreateUAV = false;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Resolve Target";
		resolveTarget.Initialize(rtInit);
	}

	{
		RenderTextureInit rtInit;
		rtInit.Width = width;
		rtInit.Height = height;
		rtInit.Format = DXGI_FORMAT_R16G16_FLOAT;
		rtInit.MSAASamples = NumSamples;
		rtInit.ArraySize = 1;
		rtInit.CreateUAV = false;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Velocity Buffer";
		velocityBuffer.Initialize(rtInit);
	}

	{
		RenderTextureInit rtInit;
		rtInit.Width = width;
		rtInit.Height = height;
		rtInit.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtInit.MSAASamples = 1;
		rtInit.ArraySize = 1;
		rtInit.CreateUAV = false;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Previous Frame";
		prevFrameTarget.Initialize(rtInit);
	}
}

void MyD3DProject::Initialize()
{
	// Check if the device supports conservative rasterization
	D3D12_FEATURE_DATA_D3D12_OPTIONS features = { };
	DX12::Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features));
	if (features.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_2)
		throw Exception("This demo requires a GPU that supports FEATURE_LEVEL_11_1 with D3D12_RESOURCE_BINDING_TIER_2");

	float aspect = float(swapChain.Width()) / swapChain.Height();
	camera.Initialize(aspect, Pi_4, 0.1f, 35.0f);

	ShadowHelper::Initialize();

	InitializeScene(AppSettings::CurrentScene);

	skybox.Initialize();

	postProcessor.Initialize();

	// Spot light and shadow bounds buffer
	{
		ConstantBufferInit cbInit;
		cbInit.Size = sizeof(LightConstants);
		cbInit.Dynamic = true;
		cbInit.CPUAccessible = false;
		cbInit.InitialState = D3D12_RESOURCE_STATE_COMMON;
		cbInit.Name = L"Spot Light Buffer";

		spotLightBuffer.Initialize(cbInit);
	}

	uint64 numCubMaps = AppSettings::NumCubeMaps;
	envMaps.Init(numCubMaps);
	for (uint64 i = 0; i < AppSettings::NumCubeMaps; ++i)
	{
		LoadTexture(envMaps[i].EnvMap, AppSettings::CubeMapPaths(i), false);
		envMaps[i].projectToSH = false;
	}

	LoadTexture(specularLookupTexture, L"Content\\Textures\\EnvSpecularLUT.dds", false);
	//GenerateEnvSpecularLookupTexture(envSpecularLUT);

	// Compile cluster visualization shaders
	fullScreenTriVS = CompileFromFile(L"Shaders\\FullScreenTriangle.hlsl", "FullScreenTriangleVS", ShaderType::Vertex);	
	
	// Compile resolve shaders
	for (uint64 msaaMode = 0; msaaMode < NumMSAAModes; msaaMode++)
	{
		CompileOptions opts;
		opts.Add("MSAASamples_", AppSettings::NumMSAASamples(MSAAModes(msaaMode)));
		resolvePS[msaaMode] = CompileFromFile(L"Shaders\\Resolve.hlsl", "ResolvePS", ShaderType::Pixel, opts);
	}

	// Jitter samples
	{
		for (uint64 jitterMode = 1; jitterMode < NumJitterModes; jitterMode++)
		{
			uint32 numSamples = AppSettings::NumJitterSamples(JitterModes(jitterMode));
			jitterSamples[jitterMode] = new Float2[numSamples];
			if (numSamples == 2)
			{
				jitterSamples[jitterMode][0] = -0.5f;
				jitterSamples[jitterMode][1] = 0.5f;
			}
			else
			{
				GenerateHaltonSamples2D(jitterSamples[jitterMode], numSamples);
			}
		}
	}

	// Resolve root signature
	{
		D3D12_ROOT_PARAMETER1 rootParameters[NumResolveRootParams] = {};

		// Standard SRV descriptors
		rootParameters[ResolveParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ResolveParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ResolveParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[ResolveParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		// Constants
		rootParameters[ResolveParams_Constants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[ResolveParams_Constants].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ResolveParams_Constants].Descriptor.RegisterSpace = 0;
		rootParameters[ResolveParams_Constants].Descriptor.ShaderRegister = 0;
		rootParameters[ResolveParams_Constants].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		// SRV descriptor indices
		rootParameters[ResolveParams_SRVIndices].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[ResolveParams_SRVIndices].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[ResolveParams_SRVIndices].Descriptor.RegisterSpace = 0;
		rootParameters[ResolveParams_SRVIndices].Descriptor.ShaderRegister = 1;
		rootParameters[ResolveParams_SRVIndices].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Linear, 0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);;
		rootSignatureDesc.pStaticSamplers = staticSamplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		DX12::CreateRootSignature(&resolveRootSignature, rootSignatureDesc);
	}
}

void MyD3DProject::Shutdown()
{
	ShadowHelper::Shutdown();
	for (uint64 i = 0; i < ArraySize_(sceneModels); ++i)
		sceneModels[i].Shutdown();
	lightCluster.Shutdown();
	meshRenderer.Shutdown();
	skybox.Shutdown();
	skyCache.Shutdown();
	postProcessor.Shutdown();
	spotLightBuffer.Shutdown();

	specularLookupTexture.Shutdown();
	for (uint64 i = 0; i < envMaps.Size(); ++i)	
		envMaps[i].EnvMap.Shutdown();
	envMaps.Shutdown();
	
	for (uint64 i = 1; i < ArraySize_(jitterSamples); i++)
	{
		delete jitterSamples[i];
		jitterSamples[i] = nullptr;
	}

	DX12::Release(resolveRootSignature);

	mainTarget.Shutdown();
	depthBuffer.Shutdown();
	resolveTarget.Shutdown();
	velocityBuffer.Shutdown();
	prevFrameTarget.Shutdown();

	pathTracer.Shutdown();
}

void MyD3DProject::CreatePSOs()
{
	ShadowHelper::CreatePSOs();
	meshRenderer.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, velocityBuffer.Texture.Format, mainTarget.MSAASamples);
	skybox.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
	lightCluster.CreatePSOs(swapChain.Format());
	
	//if (msaaEnable)
	{
		//const bool msaaEnable = AppSettings::MSAAMode != MSAAModes::MSAANone;
		const uint64 msaaModeIdx = uint64(AppSettings::MSAAMode);

		// Resolve PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = resolveRootSignature;
		psoDesc.VS = fullScreenTriVS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = mainTarget.Format();
		psoDesc.SampleDesc.Count = 1;

		psoDesc.PS = resolvePS[msaaModeIdx].ByteCode();
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resolvePSO)));
	}

	postProcessor.CreatePSOs();
	pathTracer.CreatePSOs();
}

void MyD3DProject::DestroyPSOs()
{
	ShadowHelper::DestroyPSOs();
	meshRenderer.DestroyPSOs();
	skybox.DestoryPSOs();
	postProcessor.DestroyPSOs();
	lightCluster.DestroyPSOs();

	DX12::DeferredRelease(resolvePSO);

	pathTracer.DestroyPSOs();
}

void MyD3DProject::InitializeScene(Scenes currentScene)
{
	// Load the scenes
	const uint64 currSceneIdx = uint64(currentScene);
	if (currSceneIdx == uint64(Scenes::BoxTest))
	{
		sceneModels[currSceneIdx].GenerateTestScene();
	}
	else
	{
		ModelLoadSettings settings;
		settings.FilePath = AppSettings::ScenePaths[currSceneIdx];
		settings.ForceSRGB = true;
		settings.SceneScale = AppSettings::SceneScales[currSceneIdx];
		settings.MergeMeshes = false;
		sceneModels[currSceneIdx].CreateWithAssimp(settings);
	}

	lightCluster.Shutdown();
	meshRenderer.Shutdown();
	pathTracer.Shutdown();
	DX12::FlushGPU();

	currentModel = &sceneModels[currSceneIdx];

	lightCluster.Initialize(&(currentModel->ModelSpotLights()));
	meshRenderer.Initialize(currentModel);
	pathTracer.Initialize(currentModel);

	lightCluster.CreateClusterBuffer(swapChain.Width(), swapChain.Height());
	pathTracer.CreateRenderTarget(swapChain.Width(), swapChain.Height());

	camera.SetPosition(AppSettings::SceneCameraPositions[currSceneIdx]);
	camera.SetXRotation(AppSettings::SceneCameraRotations[currSceneIdx].x);
	camera.SetYRotation(AppSettings::SceneCameraRotations[currSceneIdx].y);
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
	
	UpdateJitter();

	appViewMatrix = camera.ViewMatrix();

	lightCluster.UpdateLights(camera);

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

	{
		if (rtCurrCamera.Position() != camera.Position() 
			|| rtCurrCamera.Orientation() != camera.Orientation() 
			|| rtCurrCamera.ProjectionMatrix() != camera.ProjectionMatrix())
			rtShouldRestartPathTrace = true;

		rtCurrCamera = camera;
	}

	// Toggle VSYNC
	if (kbState.IsKeyDown(KeyboardState::V))
		swapChain.SetVSYNCEnabled(swapChain.VSYNEnabled() ? false : true);

	if (mainTarget.MSAASamples != AppSettings::NumMSAASamples())
	{
		DestroyPSOs();
		CreateRenderTargets();
		CreatePSOs();
	}

	if (currentModel != &sceneModels[(uint32)AppSettings::CurrentScene])
	{
		DestroyPSOs();
		InitializeScene(AppSettings::CurrentScene);
		CreatePSOs();
	}
}

void MyD3DProject::UpdateJitter()
{
	camera.SetAspectRatio(camera.AspectRatio());
	Float2 jitter = 0.0f;

	if (AppSettings::EnableTemporalAA)
	{
		const JitterModes jitterMode = AppSettings::JitterMode;
		uint64 numJitterSamples = AppSettings::NumJitterSamples(jitterMode);
		uint64 idx = DX12::CurrentCPUFrame % numJitterSamples;
		jitter = jitterSamples[(uint64)jitterMode][idx] - 0.5f;

		const float offsetX = jitter.x * (2.0f / mainTarget.Width());
		const float offsetY = jitter.y * (2.0f / mainTarget.Height());

		Float4x4 offsetMatrix = Float4x4::TranslationMatrix(Float3(offsetX, -offsetY, 0.0f));
		camera.SetProjection(camera.ProjectionMatrix() * offsetMatrix);
	}

	jitterOffset = (jitter - prevJitter);
	prevJitter = jitter;
}

void MyD3DProject::Render(const Timer& timer)
{
	if (AppSettings::EnableRayTracing)
		RenderRayTracing();
	else
		RenderRasterizing();
}

void MyD3DProject::RenderRayTracing()
{
	ID3D12GraphicsCommandList4* cmdList = DX12::CmdList;

	// Update the light constant buffer
	const Array<SpotLight>& spotLights = currentModel->SpotLights();
	const uint64 numSpotLights = Min<uint64>(spotLights.Size(), MaxSpotLights);

	spotLightBuffer.UpdateData(spotLights.Data(), spotLights.MemorySize(), 0);

	// Render the main forward pass
	Texture& envCubMap = skyCache.CubeMap;
	if ((uint32)AppSettings::SkyMode >= AppSettings::CubeMapStart)
		envCubMap = envMaps[(uint32)AppSettings::SkyMode - AppSettings::CubeMapStart].EnvMap;

	RayTraceData rtData;
	rtData.SkyCache = &skyCache;
	rtData.EnvCubMap = &envCubMap;

	rtData.SqrtNumSamples = 40;
	rtData.NumLights = uint32(numSpotLights);
	rtData.MaxPathLength = 3;
	rtData.MaxAnyHitPathLength = 1;

	rtData.SpotLightBuffer = &spotLightBuffer;
	rtData.MaterialBuffer = &currentModel->MaterialTextureIndicesBuffer();

	rtData.ShouldRestartPathTrace = rtShouldRestartPathTrace;

	pathTracer.Render(cmdList, camera, rtData);

	rtShouldRestartPathTrace = false;

	const RenderTexture& finalRT = pathTracer.RenderTarget();

	postProcessor.Render(cmdList, finalRT, swapChain.BackBuffer());
}

void MyD3DProject::RenderRasterizing()
{
	ID3D12GraphicsCommandList4* cmdList = DX12::CmdList;

	lightCluster.RenderClusters(cmdList, camera);

	meshRenderer.RenderSunShadowMap(cmdList, camera);

	// Update the light constant buffer
	{
		meshRenderer.RenderSpotLightShadowMap(cmdList, camera);

		const Array<SpotLight>& spotLights = meshRenderer.SpotLights();
		const void* srcData[2] = { spotLights.Data(), meshRenderer.SpotLightShadowMatrices() };
		uint64 sizes[2] = { spotLights.MemorySize(),  spotLights.Size() * sizeof(Float4x4) };
		uint64 offsets[2] = { 0, sizeof(SpotLight) * MaxSpotLights };
		spotLightBuffer.MultiUpdateData(srcData, sizes, offsets, ArraySize_(srcData));
	}

	RenderForward();

	RenderResolve();

	{
		const RenderTexture& finalRT = resolveTarget;
		postProcessor.Render(cmdList, finalRT, swapChain.BackBuffer());
	}
	
	if (AppSettings::ShowClusterVisualizer)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
		cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

		Float2 displaySize = Float2((float)swapChain.Width(), (float)swapChain.Height());
		lightCluster.RenderClusterVisualizer(cmdList, camera, displaySize);
	}
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
		barriers[2].Transition.pResource = velocityBuffer.Resource();
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[2].Transition.Subresource = 0;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = { mainTarget.RTV, velocityBuffer.RTV };
	cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	cmdList->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(depthBuffer.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	DX12::SetViewport(cmdList, mainTarget.Width(), mainTarget.Height());

	// Render the main forward pass
	Texture& specularCubMap = skyCache.CubeMap;
	if ((uint32)AppSettings::SkyMode >= AppSettings::CubeMapStart)
		specularCubMap = envMaps[(uint32)AppSettings::SkyMode - AppSettings::CubeMapStart].EnvMap;

	MainPassData mainPassData;
	mainPassData.SkyCache = &skyCache;
	mainPassData.SpecularLUT = &specularLookupTexture;
	mainPassData.SpecularCubMap = &specularCubMap;
	mainPassData.EnvSH = envSH;
	
	mainPassData.NumXTiles = lightCluster.NumXTiles();
	mainPassData.NumYTiles = lightCluster.NumYTiles();
	mainPassData.ClusterTileSize = ClusterTileSize;
	mainPassData.NumZTiles = NumZTiles;

	mainPassData.RTSize.x = (float)mainTarget.Width();
	mainPassData.RTSize.y = (float)mainTarget.Height();
	mainPassData.JitterOffset = jitterOffset;

	mainPassData.SpotLightBuffer = &spotLightBuffer;;
	mainPassData.SpotLightClusterBuffer = &lightCluster.SpotLightClusterBuffer();
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
		barriers[2].Transition.pResource = velocityBuffer.Resource();
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[2].Transition.Subresource = 0;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}
}

void MyD3DProject::RenderResolve()
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	PIXMarker pixMarker(cmdList, "MSAA And TAA Resolve");

	resolveTarget.MakeWritable(cmdList);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[1] = { resolveTarget.RTV };
	cmdList->OMSetRenderTargets(ArraySize_(rtvs), rtvs, false, nullptr);
	DX12::SetViewport(cmdList, resolveTarget.Width(), resolveTarget.Height());

	cmdList->SetGraphicsRootSignature(resolveRootSignature);
	cmdList->SetPipelineState(resolvePSO);

	DX12::BindStandardDescriptorTable(cmdList, ResolveParams_StandardDescriptors, CmdListMode::Graphics);

	ResolveConstants resolveConstants;
	float width = static_cast<float>(mainTarget.Width());
	float height = static_cast<float>(mainTarget.Height());
	resolveConstants.FilterType = static_cast<uint32>(AppSettings::ResolveFilter);
	resolveConstants.SampleRadius = static_cast<uint32>(AppSettings::ResolveFilterRadius + 0.499f);
	resolveConstants.TextureSize = Float2(width, height);
	resolveConstants.FilterRadius = AppSettings::ResolveFilterRadius;
	resolveConstants.FilterGaussianSigma = AppSettings::FilterGaussianSigma;
	
	resolveConstants.Exposure = AppSettings::ManualExposure;
	resolveConstants.ExposureFilterOffset = AppSettings::ExposureFilterOffset;

	resolveConstants.EnableTemporalAA = AppSettings::EnableTemporalAA;
	resolveConstants.TemporalAABlendFactor = AppSettings::TemporalAABlendFactor;
	
	DX12::BindTempConstantBuffer(cmdList, resolveConstants, ResolveParams_Constants, CmdListMode::Graphics);

	uint32 psSRVs[] =
	{
		mainTarget.SRV(),
		velocityBuffer.SRV(),
		depthBuffer.SRV(),
		prevFrameTarget.SRV(),
	};
	DX12::BindTempConstantBuffer(cmdList, psSRVs, ResolveParams_SRVIndices, CmdListMode::Graphics);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetVertexBuffers(0, 0, nullptr);

	cmdList->DrawInstanced(3, 1, 0, 0);

	//resolveTarget.MakeReadable(cmdList);

	{
		D3D12_RESOURCE_BARRIER barriers[2] = {};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = resolveTarget.Resource();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barriers[0].Transition.Subresource = 0;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = prevFrameTarget.Resource();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[1].Transition.Subresource = 0;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}

	cmdList->CopyResource(prevFrameTarget.Resource(), resolveTarget.Resource());

	{
		D3D12_RESOURCE_BARRIER barriers[2] = {};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = resolveTarget.Resource();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[0].Transition.Subresource = 0;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = prevFrameTarget.Resource();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[1].Transition.Subresource = 0;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MyD3DProject app(lpCmdLine);
	app.Run();
	return 0;
}