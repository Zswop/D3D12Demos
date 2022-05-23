#pragma once

#include "PCH.h"

#include "App.h"
#include "Graphics\\Model.h"
#include "Graphics\\Camera.h"
#include "Graphics\\Skybox.h"
#include "Graphics\\PostProcessor.h"
#include "Graphics\\LightCluster.h"

#include "AppSettings.h"
#include "MeshRenderer.h"

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

	LightCluster lightCluster;
	ConstantBuffer spotLightBuffer;

	RenderTexture mainTarget;
	DepthBuffer depthBuffer;
	RenderTexture resolveTarget;

	RenderTexture velocityBuffer;
	RenderTexture prevFrameTarget;
	Float2 jitterOffset;
	Float2 prevJitter;

	Texture specularLookupTexture;
	Array<EnvironmentMap> envMaps;

	CompiledShaderPtr fullScreenTriVS;
	CompiledShaderPtr resolvePS[NumMSAAModes];
	ID3D12RootSignature* resolveRootSignature = nullptr;
	ID3D12PipelineState* resolvePSO = nullptr;

	Float2* jitterSamples[NumJitterModes];
	
	void CreateRenderTargets();
	void InitializeScene(Scenes scene);

	void UpdateJitter();
	void RenderForward();
	RenderTexture& RenderResolve();

public:
	 MyD3DProject(const wchar* cmdLine);
};