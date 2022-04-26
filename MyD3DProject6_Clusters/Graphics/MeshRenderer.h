#pragma once

#include "..\\PCH.h"
#include "Model.h"
#include "Camera.h"
#include "ShaderCompilation.h"
#include "SH.h"
#include "ShadowHelper.h"

#include "..\\AppSettings.h"

namespace Framework
{
// forward declares
struct SkyCache;

struct MainPassData
{
	const SkyCache* SkyCache = nullptr;
	SH9Color EnvSH;

	const ConstantBuffer* SpotLightBuffer = nullptr;
	const RawBuffer* SpotLightClusterBuffer = nullptr;
};

struct ShadingConstants
{
	Float4Align Float3 SunDirectionWS;
	float CosSunAngularRadius = 0.0f;
	Float4Align Float3 SunIrradiance;
	float SinSunAngularRadius = 0.0f;
	Float4Align Float3 CameraPosWS;
	float ShadowNormalBias = 0.01f;

	uint32 NumXTiles = 0;
	uint32 NumXYTiles = 0;
	float NearClip = 0.0f;
	float FarClip = 0.0f;

	Float4Align ShaderSH9Color EnvSH;
};

struct MaterialTextureIndices
{
	uint32 Albedo;
	uint32 Normal;
	uint32 Roughness;
	uint32 Metallic;
};

struct SpotLight
{
	Float3 Position;
	float AngularAttenuationX;
	Float3 Direction;
	float AngularAttenuationY;
	Float3 Intensity;
	float Range;
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

	void RenderSunShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);	
	void RenderSpotLightShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

	const DepthBuffer& SunShadowMap() const { return sunShadowMap; }
	const DepthBuffer& SpotLightShadowMap() const { return spotLightShadowMap; }
	const Float4x4* SpotLightShadowMatrices() const { return spotLightShadowMatrices; }
	const StructuredBuffer& MaterialTextureIndicesBuffer() const { return materialTextureIndices; }

protected:

	void RenderDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera, ID3D12PipelineState* pso, uint64 numVisible);
	void RenderSunShadowDepth(ID3D12GraphicsCommandList* cmdList, const OrthographicCamera& camera);
	void RenderSpotLightShadowDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

	void LoadShaders();
	
	const Model* model = nullptr;

	DepthBuffer sunShadowMap;
	DepthBuffer spotLightShadowMap;
	Float4x4 spotLightShadowMatrices[AppSettings::MaxSpotLights];

	StructuredBuffer materialTextureIndices;

	CompiledShaderPtr meshVS;
	CompiledShaderPtr meshPSForward;
	ID3D12PipelineState* mainPassPSO = nullptr;
	ID3D12RootSignature* mainPassRootSignature = nullptr;

	ID3D12PipelineState* depthPSO = nullptr;
	ID3D12PipelineState* sunShadowPSO = nullptr;
	ID3D12PipelineState* spotLightShadowPSO = nullptr;
	ID3D12RootSignature* depthRootSignature = nullptr;

	Array<DirectX::BoundingBox> meshBoundingBoxes;
	Array<uint32> meshDrawIndices;
	Array<float> meshZDepths;

	SunShadowConstants sunShadowConstants;
};

}