#pragma once

#include "PCH.h"

#include "App.h"
#include "Graphics\\Model.h"
#include "Graphics\\Camera.h"
#include "Graphics\\MeshRenderer.h"

using namespace Framework;

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

	Model sceneModel;
	Model* currentModel = nullptr;
	MeshRenderer meshRenderer;

	DepthBuffer depthBuffer;
	void CreateRenderTargets();

public:
	MyD3DProject(const wchar* cmdLine);
};