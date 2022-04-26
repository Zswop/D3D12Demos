#pragma once

#include "PCH.h"

#include "App.h"
#include "Graphics\\ShaderCompilation.h"
#include "Graphics\\Model.h"

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

	DepthBuffer depthBuffer;

	Model boxModel;	

	CompiledShaderPtr colorVS;
	CompiledShaderPtr colorPS;

	ID3D12RootSignature* colorRootSignature = nullptr;
	ID3D12PipelineState* colorPSO = nullptr;

	void CreateRenderTargets();

public:
	MyD3DProject(const wchar* cmdLine);
};