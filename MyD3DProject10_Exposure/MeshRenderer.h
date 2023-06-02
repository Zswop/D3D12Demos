#pragma once

#include "PCH.h"
#include "Graphics\\Model.h"
#include "Graphics\\Camera.h"
#include "Graphics\\ShaderCompilation.h"
#include "Graphics\\SH.h"
#include "Graphics\\ShadowHelper.h"

#include "AppSettings.h"

namespace Framework
{
// forward declares
struct SkyCache;

struct MainPassData
{
	const SkyCache* SkyCache = nullptr;

	const Texture* SpecularLUT = nullptr;
	const Texture* SpecularCubMap = nullptr;
	SH9Color EnvSH;
	
	uint64 NumZTiles = 0;
	uint64 NumXTiles = 0;
	uint64 NumYTiles = 0;
	uint64 ClusterTileSize = 0;
	
	bool32 RenderLights = false;
	bool32 EnableDirectLighting = true;
	bool32 EnableIndirectLighting = true;

	float NormalMapIntensity = 0.5;
	float RoughnessScale = 1.0;

	Float2 RTSize;
	Float2 JitterOffset;

	const ConstantBuffer* SpotLightBuffer = nullptr;
	const RawBuffer* SpotLightClusterBuffer = nullptr;
};

struct ShadingConstants
{
	Float4Align Float3 SunDirectionWS;
	float CosSunAngularRadius = 0.0f;
	Float4Align Float3 SunIlluminance;
	float SinSunAngularRadius = 0.0f;
	Float4Align Float3 CameraPosWS;
	float ShadowNormalBias = 0.01f;

	uint32 NumZTiles = 0;
	uint32 NumXTiles = 0;
	uint32 NumXYTiles = 0;
	uint32 ClusterTileSize = 0;

	bool32 RenderLights = false;
	bool32 EnableDirectLighting = true;
	bool32 EnableIndirectLighting = true;
	bool32 Padding0 = false;

	float NormalMapIntensity = 0.5;
	float RoughnessScale = 1.0;
	float NearClip = 0.0f;
	float FarClip = 0.0f;

	Float2 RTSize;
	Float2 JitterOffset;

	Float4Align ShaderSH9Color EnvSH;
};

class MeshRenderer
{

public:

	MeshRenderer();

	void Initialize(const Model* sceneModel);
	void Shutdown();

	void CreatePSOs(DXGI_FORMAT mainRTFormat, DXGI_FORMAT depthFormat, DXGI_FORMAT velocityFormat, uint32 numMSAASamples);
	void DestroyPSOs();

	void RenderMainPass(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const MainPassData& mainPassData);

	void RenderSunShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);	
	void RenderSpotLightShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

	//void RenderVelocity(ID3D12GraphicsCommandList* cmdList, const DepthBuffer& depthBuffer);

	const Array<SpotLight>& SpotLights() const { return model->SpotLights(); }

	const DepthBuffer& SunShadowMap() const { return sunShadowMap; }
	const DepthBuffer& SpotLightShadowMap() const { return spotLightShadowMap; }
	const Float4x4* SpotLightShadowMatrices() const { return spotLightShadowMatrices; }

protected:

	void RenderDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera, ID3D12PipelineState* pso, uint64 numVisible);
	void RenderSunShadowDepth(ID3D12GraphicsCommandList* cmdList, const OrthographicCamera& camera);
	void RenderSpotLightShadowDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

	void LoadShaders();
	
	const Model* model = nullptr;

	DepthBuffer sunShadowMap;
	DepthBuffer spotLightShadowMap;
	Float4x4 spotLightShadowMatrices[MaxSpotLights];

	CompiledShaderPtr meshVS;
	CompiledShaderPtr meshPSForward;
	CompiledShaderPtr meshAlphaTestPS;
	ID3D12PipelineState* mainPassPSO = nullptr;
	ID3D12PipelineState* mainPassAlphaTestPSO = nullptr;
	ID3D12RootSignature* mainPassRootSignature = nullptr;

	ID3D12PipelineState* depthPSO = nullptr;
	ID3D12PipelineState* sunShadowPSO = nullptr;
	ID3D12PipelineState* spotLightShadowPSO = nullptr;
	ID3D12RootSignature* depthRootSignature = nullptr;

	Array<DirectX::BoundingBox> meshBoundingBoxes;
	Array<uint32> meshDrawIndices;
	Array<float> meshZDepths;

	SunShadowConstants sunShadowConstants;

	Float4x4 prevWorldViewProjection;

	PixelShaderPtr fullScreenTriVS;
	CompiledShaderPtr velocityPS;
	ID3D12PipelineState* velocityPSO = nullptr;
	ID3D12RootSignature* velocityRootSignature = nullptr;	
};

}