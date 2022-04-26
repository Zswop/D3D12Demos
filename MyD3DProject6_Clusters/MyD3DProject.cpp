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

static const float SpotLightIntensityFactor = 25.0f;

struct LightConstants
{
	SpotLight Lights[AppSettings::MaxSpotLights];
	Float4x4 ShadowMatrices[AppSettings::MaxSpotLights];
};

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
	
	AppSettings::NumXTiles = (width + (AppSettings::ClusterTileSize - 1)) / AppSettings::ClusterTileSize;
	AppSettings::NumYTiles = (height + (AppSettings::ClusterTileSize - 1)) / AppSettings::ClusterTileSize;
	const uint64 numXYZTiles = AppSettings::NumXTiles * AppSettings::NumYTiles * AppSettings::NumZTiles;

	{
		// SpotLight cluster bitmask buffer
		RawBufferInit rbInit;
		rbInit.NumElements = numXYZTiles * AppSettings::SpotLightElementsPerCluster;
		rbInit.CreateUAV = true;
		rbInit.InitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		rbInit.Name = L"Spot Light Cluster Buffer";
		spotLightClusterBuffer.Initialize(rbInit);
	}
}

void MyD3DProject::Initialize()
{
	ShadowHelper::Initialize();

	// Load the scenes
	for (uint64 i = 0; i < uint64(Scenes::NumValues); ++i)
	{
		ModelLoadSettings settings;
		settings.FilePath = AppSettings::ScenePaths[i];
		settings.ForceSRGB = true;
		settings.SceneScale = AppSettings::SceneScales[i];
		settings.MergeMeshes = false;
		sceneModels[i].CreateWithAssimp(settings);
	}

	float aspect = float(swapChain.Width()) / swapChain.Height();
	camera.Initialize(aspect, Pi_4, 0.1f, 35.0f);

	InitializeScene(Scenes::Sponza);

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

	{
		// Spot light bounds and instance buffers
		StructuredBufferInit sbInit;
		sbInit.Stride = sizeof(ClusterBounds);
		sbInit.NumElements = AppSettings::MaxSpotLights;
		sbInit.Dynamic = true;
		sbInit.CPUAccessible = true;
		spotLightBoundsBuffer.Initialize(sbInit);		

		sbInit.Stride = sizeof(uint32);
		spotLightInstanceBuffer.Initialize(sbInit);
	}

	{
		// Spot light and shadow bounds buffer
		ConstantBufferInit cbInit;
		cbInit.Size = sizeof(LightConstants);
		cbInit.Dynamic = true;
		cbInit.CPUAccessible = false;
		cbInit.InitialState = D3D12_RESOURCE_STATE_COMMON;
		cbInit.Name = L"Spot Light Buffer";

		spotLightBuffer.Initialize(cbInit);
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

	MakeConeGeometry(AppSettings::NumConeSides, spotLightClusterVtxBuffer, spotLightClusterIdxBuffer, coneVertices);

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

		DX12::CreateRootSignature(&clusterVisRootSignature, rootSignatureDesc);
	}

	//GenerateEnvSpecularLookupTexture(envSpecularLUT);
}

void MyD3DProject::Shutdown()
{
	ShadowHelper::Shutdown();
	for (uint64 i = 0; i < ArraySize_(sceneModels); ++i)
		sceneModels[i].Shutdown();
	meshRenderer.Shutdown();
	skybox.Shutdown();
	skyCache.Shutdown();
	postProcessor.Shutdown();

	envSpecularLUT.Shutdown();
	for (uint64 i = 0; i < envMaps.Size(); ++i)	
		envMaps[i].EnvMap.Shutdown();	
	envMaps.Shutdown();

	spotLightBuffer.Shutdown();
	spotLightBoundsBuffer.Shutdown();
	spotLightClusterBuffer.Shutdown();
	spotLightInstanceBuffer.Shutdown();

	spotLightClusterVtxBuffer.Shutdown();
	spotLightClusterIdxBuffer.Shutdown();

	DX12::Release(clusterRS);
	DX12::Release(clusterVisRootSignature);

	mainTarget.Shutdown();
	depthBuffer.Shutdown();
}

void MyD3DProject::CreatePSOs()
{
	ShadowHelper::CreatePSOs();
	meshRenderer.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
	skybox.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);

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

		psoDesc.PS = clusterFrontFacePS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterFrontFacePSO)));

		psoDesc.PS = clusterBackFacePS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterBackFacePSO)));

		psoDesc.PS = clusterIntersectingPS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterIntersectingPSO)));

		clusterFrontFacePSO->SetName(L"Cluster Front-Face PSO");
		clusterBackFacePSO->SetName(L"Cluster Back-Face PSO");
		clusterIntersectingPSO->SetName(L"Cluster Intersecting PSO");
	}

	{
		// Cluster visualizer PSO
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = clusterVisRootSignature;
		psoDesc.VS = fullScreenTriVS.ByteCode();
		psoDesc.PS = clusterVisPS.ByteCode();
		psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
		psoDesc.BlendState = DX12::GetBlendState(BlendState::AlphaBlend);
		psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = swapChain.Format();
		psoDesc.SampleDesc.Count = 1;
		DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterVisPSO)));
	}
}

void MyD3DProject::DestroyPSOs()
{
	ShadowHelper::DestroyPSOs();
	meshRenderer.DestroyPSOs();
	skybox.DestoryPSOs();
	postProcessor.DestroyPSOs();

	DX12::DeferredRelease(clusterFrontFacePSO);
	DX12::DeferredRelease(clusterBackFacePSO);
	DX12::DeferredRelease(clusterIntersectingPSO);

	DX12::DeferredRelease(clusterVisPSO);
}

void MyD3DProject::InitializeScene(Scenes currentScene)
{
	currentModel = &sceneModels[uint64(currentScene)];

	meshRenderer.Shutdown();
	DX12::FlushGPU();
	meshRenderer.Initialize(currentModel);

	camera.SetPosition(AppSettings::SceneCameraPositions[uint64(currentScene)]);
	camera.SetXRotation(AppSettings::SceneCameraRotations[uint64(currentScene)].x);
	camera.SetYRotation(AppSettings::SceneCameraRotations[uint64(currentScene)].y);

	{
		// Initialize the spotlight data used for rendering
		const uint64 numSpotLights = currentModel->SpotLights().Size();
		spotLights.Init(numSpotLights);

		for (uint64 i = 0; i < numSpotLights; ++i)
		{
			const ModelSpotLight& srcLight = currentModel->SpotLights()[i];

			SpotLight& spotLight = spotLights[i];
			spotLight.Position = srcLight.Position;
			spotLight.Direction = -srcLight.Direction;
			spotLight.Intensity = srcLight.Intensity * SpotLightIntensityFactor;
			spotLight.AngularAttenuationX = std::cos(srcLight.AngularAttenuation.x * 0.5f);
			spotLight.AngularAttenuationY = std::cos(srcLight.AngularAttenuation.y * 0.5f);
			spotLight.Range = 7.5f;
		}
	}
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

	UpdateLights();

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

	RenderClusters();

	meshRenderer.RenderSunShadowMap(cmdList, camera);

	{
		meshRenderer.RenderSpotLightShadowMap(cmdList, camera);

		//spotLightBuffer.UpdateData(spotLights.Data(), spotLights.MemorySize(), 0);

		// Update the light constant buffer
		const void* srcData[2] = { spotLights.Data(), meshRenderer.SpotLightShadowMatrices() };
		uint64 sizes[2] = { spotLights.MemorySize(),  spotLights.Size() * sizeof(Float4x4) };
		uint64 offsets[2] = { 0, sizeof(SpotLight) * AppSettings::MaxSpotLights };
		spotLightBuffer.MultiUpdateData(srcData, sizes, offsets, ArraySize_(srcData));
	}

	RenderForward();

	postProcessor.Render(cmdList, mainTarget, swapChain.BackBuffer());

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
	cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

	RenderClusterVisualizer();
}

void MyD3DProject::UpdateLights()
{
	const uint64 numSpotLights = Min<uint64>(spotLights.Size(), AppSettings::MaxSpotLights);

	// This is an additional scale factor that's needed to make sure that our polygonal bounding cone
	// fully encloses the actual cone representing the light's area of influence
	const float inRadius = std::cos(Pi / AppSettings::NumConeSides);
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
	bool intersectsCamera[AppSettings::MaxSpotLights] = { };

	// Update the light bounds buffer
	for (uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; spotLightIdx++)
	{
		const SpotLight& spotLight = spotLights[spotLightIdx];
		const ModelSpotLight& srcSpotLight = currentModel->SpotLights()[spotLightIdx];

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

		bounds.ZBounds.x = uint32(minZ * AppSettings::NumZTiles);
		bounds.ZBounds.y = uint32(maxZ * AppSettings::NumZTiles);

		boundsData[spotLightIdx] = bounds;
		intersectsCamera[spotLightIdx] = SphereConeIntersection(spotLight.Position, srcSpotLight.Direction, spotLight.Range,
			srcSpotLight.AngularAttenuation.y, nearClipCenter, nearClipRadius);

		spotLights[spotLightIdx].Intensity = srcSpotLight.Intensity * SpotLightIntensityFactor;
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

void MyD3DProject::RenderClusters()
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

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
		clusterConstants.NumXTiles = uint32(AppSettings::NumXTiles);
		clusterConstants.NumYTiles = uint32(AppSettings::NumYTiles);
		clusterConstants.NumXYTiles = uint32(AppSettings::NumXTiles * AppSettings::NumYTiles);
		clusterConstants.InstanceOffset = 0;
		clusterConstants.NumLights = Min<uint32>(uint32(spotLights.Size()), uint32(AppSettings::MaxSpotLights));
		clusterConstants.NumDecals = 0;

		cmdList->OMSetRenderTargets(0, nullptr, false, nullptr);

		DX12::SetViewport(cmdList, AppSettings::NumXTiles, AppSettings::NumYTiles);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmdList->SetGraphicsRootSignature(clusterRS);

		DX12::BindStandardDescriptorTable(cmdList, ClusterParams_StandardDescriptors, CmdListMode::Graphics);

		D3D12_INDEX_BUFFER_VIEW ibView = spotLightClusterIdxBuffer.IBView();
		cmdList->IASetIndexBuffer(&ibView);

		clusterConstants.ElementsPerCluster = uint32(AppSettings::SpotLightElementsPerCluster);
		clusterConstants.InstanceOffset = 0;
		clusterConstants.BoundsBufferIdx = spotLightBoundsBuffer.SRV;
		clusterConstants.VertexBufferIdx = spotLightClusterVtxBuffer.SRV;
		clusterConstants.InstanceBufferIdx = spotLightInstanceBuffer.SRV;
		DX12::BindTempConstantBuffer(cmdList, clusterConstants, ClusterParams_CBuffer, CmdListMode::Graphics);

		D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { spotLightClusterBuffer.UAV };
		DX12::BindTempDescriptorTable(cmdList, uavs, ArraySize_(uavs), ClusterParams_UAVDescriptors, CmdListMode::Graphics);

		const uint64 numLightsToRender = Min<uint64>(spotLights.Size(), AppSettings::MaxSpotLights);
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

void MyD3DProject::RenderForward()
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	PIXMarker marker(cmdList, "Forward rendering");

	{
		// Transition depth buffers to a writable state
		D3D12_RESOURCE_BARRIER barriers[4] = {};

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

		barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[3].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[3].Transition.pResource = meshRenderer.SpotLightShadowMap().Resource();
		barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[3].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[3].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

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
	mainPassData.SpotLightBuffer = &spotLightBuffer;
	mainPassData.SpotLightClusterBuffer = &spotLightClusterBuffer;
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
		D3D12_RESOURCE_BARRIER barriers[4] = {};

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

		barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[3].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[3].Transition.pResource = meshRenderer.SpotLightShadowMap().Resource();
		barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[3].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barriers[3].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
	}
}

void MyD3DProject::RenderClusterVisualizer()
{
	ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

	PIXMarker pixMarker(cmdList, "Cluster Visualizer");

	Float2 displaySize = Float2(float(swapChain.Width()), float(swapChain.Height()));
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
	scissorRect.right = uint32(swapChain.Width());
	scissorRect.bottom = uint32(swapChain.Height());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->SetGraphicsRootSignature(clusterVisRootSignature);
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
	clusterVisConstants.NumXTiles = uint32(AppSettings::NumXTiles);
	clusterVisConstants.NumXYTiles = uint32(AppSettings::NumXTiles * AppSettings::NumYTiles);
	clusterVisConstants.DecalClusterBufferIdx = 0;
	clusterVisConstants.SpotLightClusterBufferIdx = spotLightClusterBuffer.SRV;
	DX12::BindTempConstantBuffer(cmdList, clusterVisConstants, ClusterVisParams_CBuffer, CmdListMode::Graphics);
	
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetVertexBuffers(0, 0, nullptr);

	cmdList->DrawInstanced(3, 1, 0, 0);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow){
	//FILE* fp;

	//AllocConsole();
	//freopen_s(&fp, "CONIN$", "r", stdin);
	//freopen_s(&fp, "CONOUT$", "w", stdout);
	//freopen_s(&fp, "CONOUT$", "w", stderr);

	MyD3DProject app(lpCmdLine);
	app.Run();
	return 0;
}