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

struct SunShadowConstantsBase
{
	Float4x4 ShadowMatrix;
	float CascadeSplits[NumCascades] = { };
	float CascadeTexelSizes[NumCascades] = { };
	Float4 CascadeOffsets[NumCascades];
	Float4 CascadeScales[NumCascades];
};

struct SunShadowConstants
{
	SunShadowConstantsBase Base;
	uint32 Dummy[4] = { };
};

namespace ShadowHelper
{

extern Float4x4 ShadowScaleOffsetMatrix;

void Initialize();
void Shutdown();

void CreatePSOs();
void DestroyPSOs();

extern Float4x4 ScaleOffsetMatrix;

void ConvertShadowMap(ID3D12GraphicsCommandList* cmdList, const DepthBuffer& depthMap, RenderTexture& smTarget,
	uint32 arraySlice, RenderTexture& tempTarget, float filterSizeU, float filterSizeV, const Camera& camera);

void PrepareCascades(const Float3& lightDir, uint64 shadowMapSize, bool stabilize, const Camera& camera,
		SunShadowConstantsBase& constants, OrthographicCamera* cascadeCameras);

}
}
