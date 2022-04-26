#pragma once

#include "PCH.h"

#include "App.h"
#include "Graphics\\Model.h"
#include "Graphics\\Camera.h"
#include "Graphics\\MeshRenderer.h"
#include "Graphics\\Skybox.h"
#include "AppSettings.h"

#include "PostProcessor.h"

using namespace Framework;

struct EnvironmentMap
{
	std::wstring Name;
	Texture EnvMap;
	SH9Color SH;

	bool projectToSH;
};

class MyD3DProject : public App
{
protected:
	virtual void Initialize() override;
	virtual void Shutdown() override;

	virtual void Update(const Timer& timer) override;
	virtual void Render(const Timer& timer) override;

	virtual void BeforeReset() override;
	virtual void AfterReset() override;

	virtual void CreatePSOs() override;
	virtual void DestroyPSOs() override;

	FirstPersonCamera camera;

	Skybox skybox;
	SkyCache skyCache;
	SH9Color envSH;

	PostProcessor postProcessor;
	
	// Model
	Model sceneModels[uint64(Scenes::NumValues)];
	const Model* currentModel = nullptr;
	MeshRenderer meshRenderer;

	Texture envSpecularLUT;

	RenderTexture mainTarget;
	DepthBuffer depthBuffer;

	Array<EnvironmentMap> envMaps;

	StructuredBuffer spotLightClusterVtxBuffer;
	FormattedBuffer spotLightClusterIdxBuffer;
	Array<Float3> coneVertices;

	Array<SpotLight> spotLights;
	ConstantBuffer spotLightBuffer;
	StructuredBuffer spotLightBoundsBuffer;
	StructuredBuffer spotLightInstanceBuffer;
	RawBuffer spotLightClusterBuffer;
	uint64 numIntersectingSpotLights = 0;

	ID3D12RootSignature* clusterRS = nullptr;
	CompiledShaderPtr clusterVS;
	CompiledShaderPtr clusterFrontFacePS;
	CompiledShaderPtr clusterBackFacePS;
	CompiledShaderPtr clusterIntersectingPS;
	ID3D12PipelineState* clusterFrontFacePSO = nullptr;
	ID3D12PipelineState* clusterBackFacePSO = nullptr;
	ID3D12PipelineState* clusterIntersectingPSO = nullptr;

	CompiledShaderPtr fullScreenTriVS;
	CompiledShaderPtr clusterVisPS;
	ID3D12RootSignature* clusterVisRootSignature = nullptr;
	ID3D12PipelineState* clusterVisPSO = nullptr;

	void CreateRenderTargets();
	void InitializeScene(Scenes scene);

	void UpdateLights();
	void RenderClusters();
	void RenderForward();
	void RenderClusterVisualizer();

public:
	 MyD3DProject(const wchar* cmdLine);
};