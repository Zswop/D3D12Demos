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
	
	Model sceneModel;
	Model* currentModel = nullptr;
	MeshRenderer meshRenderer;

	Texture envSpecularLUT;

	RenderTexture mainTarget;
	DepthBuffer depthBuffer;

	Array<EnvironmentMap> envMaps;

	void CreateRenderTargets();

	void RenderForward();

public:
	 MyD3DProject(const wchar* cmdLine);
};