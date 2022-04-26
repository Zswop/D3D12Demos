#pragma once

#include "..\\PCH.h"
#include "Model.h"
#include "Camera.h"
#include "ShaderCompilation.h"
#include "SH.h"

namespace Framework
{
// forward declares
struct SkyCache;

struct MainPassData
{
	const SkyCache* SkyCache = nullptr;
	SH9Color EnvSH;
};

struct ShadingConstants
{
	Float4Align Float3 SunDirectionWS;
	float CosSunAngularRadius = 0.0f;
	Float4Align Float3 SunIrradiance;
	float SinSunAngularRadius = 0.0f;
	Float4Align Float3 CameraPosWS;

	Float4Align ShaderSH9Color EnvSH;
};

class MeshRenderer
{

public:

	MeshRenderer();

	void Initialize(const Model* sceneModel);
	void Shutdown();

	void CreatePSOs(DXGI_FORMAT mainRTFormat, DXGI_FORMAT depthFormat, uint32 numMSAASamples);
	void DestroyPSOs();

	void RenderMainPass(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const MainPassData& mainPassData);

protected:

	void LoadShaders();
	
	const Model* model = nullptr;
	
	StructuredBuffer materialTextureIndices;

	CompiledShaderPtr meshVS;
	CompiledShaderPtr meshPSForward;
	ID3D12PipelineState* mainPassPSO = nullptr;
	ID3D12RootSignature* mainPassRootSignature = nullptr;
};

}