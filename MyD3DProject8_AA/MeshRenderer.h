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

	Float2 RTSize;
	Float2 JitterOffset;

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

	uint32 NumZTiles = 0;
	uint32 NumXTiles = 0;
	uint32 NumXYTiles = 0;
	uint32 ClusterTileSize = 0;

	float NearClip = 0.0f;
	float FarClip = 0.0f;
	float Padding0 = 0.0;
	float Padding1 = 0.0;

	Float2 RTSize;
	Float2 JitterOffset;

	Float4Align ShaderSH9Color EnvSH;
};

struct MaterialTextureIndices
{
	uint32 Albedo;
	uint32 Normal;
	uint32 Roughness;
	uint32 Metallic;
	uint32 Opacity;
	uint32 Emissive;
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
	Float4x4 spotLightShadowMatrices[MaxSpotLights];

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

	Float4x4 prevWorldViewProjection;

	PixelShaderPtr fullScreenTriVS;
	CompiledShaderPtr velocityPS;
	ID3D12PipelineState* velocityPSO = nullptr;
	ID3D12RootSignature* velocityRootSignature = nullptr;	
};

}