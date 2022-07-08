#pragma once

#include "..\\PCH.h"
#include "..\\F12_Math.h"

namespace Framework
{

class Camera;
class OrthographicCamera;
class PerspectiveCamera;
struct DepthBuffer;
struct RenderTexture;
	
const uint64 NumCascades = 4;
const float MaxShadowFilterSize = 9.0f;

enum class ShadowMapMode : uint32
{
	DepthMap,
	EVSM2,
	EVSM4,
	VSM,

	NumValues,
};

struct SunShadowConstantsBase
{
	Float4x4 ShadowMatrix;
	float CascadeSplits[NumCascades] = { };
	float CascadeTexelSizes[NumCascades] = { };
	Float4 CascadeOffsets[NumCascades];
	Float4 CascadeScales[NumCascades];
};

struct SunShadowConstantsDepthMap
{
	SunShadowConstantsBase Base;
	uint32 Dummy[4] = { };
};

struct EVSMConstants
{
	float PositiveExponent = 0.0f;
	float NegativeExponent = 0.0f;
	float LightBleedingReduction = 0.25f;
	float Bias = 0.001f;
};

struct SunShadowConstantsEVSM
{
	SunShadowConstantsBase Base;
	EVSMConstants EVSM;
};

struct SunShadowConstants
{
	SunShadowConstantsBase Base;
	Float4 Extra;
};

namespace ShadowHelper
{

extern Float4x4 ShadowScaleOffsetMatrix;
extern Float4x4 ScaleOffsetMatrix;

void Initialize(ShadowMapMode smMode);
void Shutdown();

void CreatePSOs();
void DestroyPSOs();

DXGI_FORMAT SMFormat(ShadowMapMode smMode);

void ConvertShadowMap(ID3D12GraphicsCommandList* cmdList, const DepthBuffer& depthMap, RenderTexture& smTarget,
	uint32 arraySlice, RenderTexture& tempTarget, float filterSizeU, float filterSizeV,	bool32 use3x3Filter = true,
	float positiveExponent = 0.0f, float negativeExponent = 0.0f);

void PrepareCascades(const Float3& lightDir, uint64 shadowMapSize, bool stabilize, const Camera& camera,
		SunShadowConstantsBase& constants, OrthographicCamera* cascadeCameras);
}
}
