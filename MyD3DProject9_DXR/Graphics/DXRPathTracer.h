#pragma once

#include "..\\PCH.h"
#include "GraphicsTypes.h"
#include "ShaderCompilation.h"
#include "Camera.h"
#include "Model.h"

namespace Framework
{

// forward declares
struct SkyCache;

struct RayTraceData
{
	const SkyCache* SkyCache = nullptr;
	const Texture* EnvCubMap = nullptr;

	uint32 SqrtNumSamples = 0;
	uint32 NumLights = 0;

	uint32 MaxPathLength = 3;
	uint32 MaxAnyHitPathLength = 1;
	bool ShouldRestartPathTrace = false;

	const ConstantBuffer* SpotLightBuffer = nullptr;
	const StructuredBuffer* MaterialBuffer = nullptr;
};

class DXRPathTracer
{

public:

	DXRPathTracer();

	void Initialize(const Model* sceneModel);
	void Shutdown();

	void CreateRenderTarget(uint32 width, uint32 height);

	void CreatePSOs();
	void DestroyPSOs();

	void Render(ID3D12GraphicsCommandList4* cmdList, const Camera& camera, const RayTraceData& rtData);

	const RenderTexture& RenderTarget() const { return rtTarget; }

protected:

	void BuildRTAccelerationStructure();

	const Model* currentModel = nullptr;

	RenderTexture rtTarget;

	CompiledShaderPtr rayTraceLib;
	ID3D12RootSignature* rtRootSignature = nullptr;
	ID3D12StateObject* rtPSO = nullptr;
	
	StructuredBuffer rtRayGenTable;
	StructuredBuffer rtHitTable;
	StructuredBuffer rtMissTable;
	StructuredBuffer rtGeoInfoBuffer;

	RawBuffer rtBottomLevelAccelStructure;
	RawBuffer rtTopLevelAccelStructure;

	bool buildAccelStructure = true;
	uint64 lastBuildAccelStructureFrame = uint64(-1);
	uint32 rtCurrSampleIdx = 0;
};

}