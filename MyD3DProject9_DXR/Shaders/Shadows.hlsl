#include "EVSM.hlsl"

#define ShadowMapMode_DepthMap_ 0
#define ShadowMapMode_EVSM_     1

#ifndef ShadowMapMode_
	#define ShadowMapMode_ ShadowMapMode_DepthMap_
#endif

//=================================================================================================
// Constants
//=================================================================================================

static const uint NumCascades = 4;

struct SunShadowConstantsBase
{
	row_major float4x4 ShadowMatrix;
	float4 CascadeSplits;
	float4 CascadeTexelSizes;
	float4 CascadeOffsets[NumCascades];
	float4 CascadeScales[NumCascades];
};

struct SunShadowConstantsDepthMap
{
	SunShadowConstantsBase Base;
	uint4 Extra;
};

struct EVSMConstants
{
	float PositiveExponent;
	float NegativeExponent;
	float LightBleedingReduction;
	uint Padding;
};

struct SunShadowConstantsEVSM
{
	SunShadowConstantsBase Base;
	EVSMConstants Extra;
};

#if ShadowMapMode_ == ShadowMapMode_DepthMap_
	#define SunShadowConstants SunShadowConstantsDepthMap
	typedef uint4 ExtraShadowConstants;
	typedef SamplerComparisonState ShadowSampler;
#elif ShadowMapMode_ == ShadowMapMode_EVSM_
	#define SunShadowConstants SunShadowConstantsEVSM
	typedef EVSMConstants ExtraShadowConstants;
	typedef SamplerState ShadowSampler;
#endif

ShadowSampler GetShadowSampler(SamplerComparisonState shadowMapSampler, SamplerState anisoSampler)
{
#if ShadowMapMode_ == ShadowMapMode_DepthMap_
	ShadowSampler shadowSampler = shadowMapSampler;
#elif ShadowMapMode_ == ShadowMapMode_EVSM_
	ShadowSampler shadowSampler = anisoSampler;
#endif
	return shadowSampler;
}

//-------------------------------------------------------------------------------------------------
// Samples the EVSM shadow map with explicit gradients
//-------------------------------------------------------------------------------------------------
float SampleShadowMapEVSM(in float3 shadowPos, in float3 shadowPosDX, in float3 shadowPosDY, in uint arrayIdx,
	in Texture2DArray shadowMap, in SamplerState evsmSampler, in EVSMConstants evsmConstants, in float3 cascadeScale)
{
	float2 exponents = GetEVSMExponents(evsmConstants.PositiveExponent, evsmConstants.NegativeExponent, cascadeScale);
	float2 warpedDepth = WarpDepth(shadowPos.z, exponents);

	float4 occluder = shadowMap.SampleGrad(evsmSampler, float3(shadowPos.xy, arrayIdx), shadowPosDX.xy, shadowPosDY.xy);

	// Derivative of warping at depth
	float2 depthScale = 0.0001f * exponents * warpedDepth;
	float2 minVariance = depthScale * depthScale;

	float posContrib = ChebyshevUpperBound(occluder.xz, warpedDepth.x, minVariance.x, evsmConstants.LightBleedingReduction);
	float negContrib = ChebyshevUpperBound(occluder.yw, warpedDepth.y, minVariance.y, evsmConstants.LightBleedingReduction);
	float shadowContrib = min(posContrib, negContrib);

	return shadowContrib;
}

//-------------------------------------------------------------------------------------------------
// Samples the EVSM shadow map with implicit gradients
//-------------------------------------------------------------------------------------------------
float SampleShadowMapEVSM(in float3 shadowPos, in uint arrayIdx, in Texture2DArray shadowMap, in SamplerState evsmSampler,
	in EVSMConstants evsmConstants, in float3 cascadeScale)
{
	float2 exponents = GetEVSMExponents(evsmConstants.PositiveExponent, evsmConstants.NegativeExponent, cascadeScale);
	float2 warpedDepth = WarpDepth(shadowPos.z, exponents);

	float4 occluder = shadowMap.Sample(evsmSampler, float3(shadowPos.xy, arrayIdx));

	// Derivative of warping at depth
	float2 depthScale = 0.0001f * exponents * warpedDepth;
	float2 minVariance = depthScale * depthScale;

	float posContrib = ChebyshevUpperBound(occluder.xz, warpedDepth.x, minVariance.x, evsmConstants.LightBleedingReduction);
	float negContrib = ChebyshevUpperBound(occluder.yw, warpedDepth.y, minVariance.y, evsmConstants.LightBleedingReduction);	
	float shadowContrib = min(posContrib, negContrib);

	return shadowContrib;
}

//-------------------------------------------------------------------------------------------------
// Helper function for SampleShadowMapPCF
//-------------------------------------------------------------------------------------------------
float SampleShadowMap(in float2 baseUV, in float u, in float v, in float2 shadowMapSizeInv, in uint arrayIdx,
	in float depth, in Texture2DArray shadowMap, in SamplerComparisonState pcfSampler)
{
	float2 uv = baseUV + float2(u, v) * shadowMapSizeInv;
	return shadowMap.SampleCmpLevelZero(pcfSampler, float3(uv, arrayIdx), depth);
}

//-------------------------------------------------------------------------------------------------
// Samples a shadow depth map with optimized PCF filtering
//-------------------------------------------------------------------------------------------------
float SampleShadowMapPCF(in float3 shadowPos, in uint arrayIdx, in Texture2DArray shadowMap, in SamplerComparisonState pcfSampler)
{
	float2 shadowMapSize;
	float numSlices;
	shadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);

	float lightDepth = shadowPos.z - 0.001f;
	float2 uv = shadowPos.xy * shadowMapSize;      // 1 uint - 1 texel
	float2 shadowMapSizeInv = 1.0f / shadowMapSize;

	float2 baseUV = floor(uv + 0.5f);
	float s = (uv.x + 0.5f - baseUV.x);
	float t = (uv.y + 0.5f - baseUV.y);

	baseUV -= float2(0.5f, 0.5f);
	baseUV *= shadowMapSizeInv;

	float sum = 0;

	#define FilterSize_ 7

#if FilterSize_ == 2
	return shadowMap.SampleCmpLevelZero(pcfSampler, float3(shadowPos.xy, arrayIdx), lightDepth);
#elif FilterSize_ == 3

	float uw0 = (3 - 2 * s);
	float uw1 = (1 + 2 * s);

	float u0 = (2 - s) / uw0 - 1;
	float u1 = s / uw1 + 1;

	float vw0 = (3 - 2 * t);
	float vw1 = (1 + 2 * t);

	float v0 = (2 - t) / vw0 - 1;
	float v1 = t / vw1 + 1;

	sum += uw0 * vw0 * SampleShadowMap(baseUV, u0, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw0 * SampleShadowMap(baseUV, u1, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw0 * vw1 * SampleShadowMap(baseUV, u0, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw1 * SampleShadowMap(baseUV, u1, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	return sum * 1.0f / 16;

#elif FilterSize_ == 5

	float uw0 = (4 - 3 * s);
	float uw1 = 7;
	float uw2 = (1 + 3 * s);

	float u0 = (3 - 2 * s) / uw0 - 2;
	float u1 = (3 + s) / uw1;
	float u2 = s / uw2 + 2;

	float vw0 = (4 - 3 * t);
	float vw1 = 7;
	float vw2 = (1 + 3 * t);

	float v0 = (3 - 2 * t) / vw0 - 2;
	float v1 = (3 + t) / vw1;
	float v2 = t / vw2 + 2;

	sum += uw0 * vw0 * SampleShadowMap(baseUV, u0, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw0 * SampleShadowMap(baseUV, u1, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw0 * SampleShadowMap(baseUV, u2, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	sum += uw0 * vw1 * SampleShadowMap(baseUV, u0, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw1 * SampleShadowMap(baseUV, u1, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw1 * SampleShadowMap(baseUV, u2, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	sum += uw0 * vw2 * SampleShadowMap(baseUV, u0, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw2 * SampleShadowMap(baseUV, u1, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw2 * SampleShadowMap(baseUV, u2, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	return sum * 1.0f / 144;

#else // FilterSize_ == 7

	float uw0 = (5 * s - 6);
	float uw1 = (11 * s - 28);
	float uw2 = -(11 * s + 17);
	float uw3 = -(5 * s + 1);

	float u0 = (4 * s - 5) / uw0 - 3;
	float u1 = (4 * s - 16) / uw1 - 1;
	float u2 = -(7 * s + 5) / uw2 + 1;
	float u3 = -s / uw3 + 3;

	float vw0 = (5 * t - 6);
	float vw1 = (11 * t - 28);
	float vw2 = -(11 * t + 17);
	float vw3 = -(5 * t + 1);

	float v0 = (4 * t - 5) / vw0 - 3;
	float v1 = (4 * t - 16) / vw1 - 1;
	float v2 = -(7 * t + 5) / vw2 + 1;
	float v3 = -t / vw3 + 3;

	sum += uw0 * vw0 * SampleShadowMap(baseUV, u0, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw0 * SampleShadowMap(baseUV, u1, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw0 * SampleShadowMap(baseUV, u2, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw3 * vw0 * SampleShadowMap(baseUV, u3, v0, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	sum += uw0 * vw1 * SampleShadowMap(baseUV, u0, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw1 * SampleShadowMap(baseUV, u1, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw1 * SampleShadowMap(baseUV, u2, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw3 * vw1 * SampleShadowMap(baseUV, u3, v1, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	sum += uw0 * vw2 * SampleShadowMap(baseUV, u0, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw2 * SampleShadowMap(baseUV, u1, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw2 * SampleShadowMap(baseUV, u2, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw3 * vw2 * SampleShadowMap(baseUV, u3, v2, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	sum += uw0 * vw3 * SampleShadowMap(baseUV, u0, v3, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw1 * vw3 * SampleShadowMap(baseUV, u1, v3, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw2 * vw3 * SampleShadowMap(baseUV, u2, v3, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);
	sum += uw3 * vw3 * SampleShadowMap(baseUV, u3, v3, shadowMapSizeInv, arrayIdx, lightDepth, shadowMap, pcfSampler);

	return sum * 1.0f / 2704;

#endif
}

//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
float3 GetShadowPosOffset(in float nDotL, in float3 normal, in float shadowMapSize)
{
	const float offsetScale = 4.0f;
	float texelSize = 2.0f / shadowMapSize;
	float nmlOffsetScale = saturate(1.0f - nDotL);	
	return texelSize * offsetScale * nmlOffsetScale * normal;
}

//--------------------------------------------------------------------------------------
// Computes the visibility for a directional light using implicit derivatives
//--------------------------------------------------------------------------------------
float SunShadowVisibility(in float3 positionWS, in float depthVS, in float3 normalWS, in float3 shadowPosOffset, in float2 uvOffset,
	in Texture2DArray sunShadowMap, in ShadowSampler shadowSampler, in SunShadowConstants shadowConstants)
{
	// Figure out which cascade to sample from
	uint cascadeIdx = 0;

	[unroll]
	for (uint i = 0; i < NumCascades - 1; ++i)
	{
		[flatten]
		if (depthVS > shadowConstants.Base.CascadeSplits[i])
			cascadeIdx = i + 1;
	}
	
	// Project into shadow space
	float3 finalOffset = shadowPosOffset / abs(shadowConstants.Base.CascadeScales[cascadeIdx].z);
	float3 shadowPos = mul(float4(positionWS + finalOffset, 1.0f), shadowConstants.Base.ShadowMatrix).xyz;

	shadowPos += shadowConstants.Base.CascadeOffsets[cascadeIdx].xyz;
	shadowPos *= shadowConstants.Base.CascadeScales[cascadeIdx].xyz;

	shadowPos.xy += uvOffset;

#if ShadowMapMode_ == ShadowMapMode_DepthMap_
	return SampleShadowMapPCF(shadowPos, cascadeIdx, sunShadowMap, shadowSampler);
#elif ShadowMapMode_ == ShadowMapMode_EVSM_
	return SampleShadowMapEVSM(shadowPos, cascadeIdx, sumShadowMap, shadowSampler, shadowConstants.Extra,
		shadowConstants.Base.CascadeScales[cascadeIdx].xyz);
#endif
}

float SpotLightShadowVisibility(in float3 positionWS, in float4x4 shadowMatrix, in uint shadowMapIdx,
	in float3 shadowPosOffset, in Texture2DArray shadowMap,	in SamplerComparisonState shadowSampler, in float2 clipPlanes,
	in ExtraShadowConstants extraConstants)
{
	const float3 posOffset = shadowPosOffset;

	// Project into shadow space
	float4 shadowPos = mul(float4(positionWS + shadowPosOffset, 1.0f), shadowMatrix);
	shadowPos.xyz /= shadowPos.w;

	return SampleShadowMapPCF(shadowPos.xyz, shadowMapIdx, shadowMap, shadowSampler);
}