//=================================================================================================
// Includes
//=================================================================================================
#include "EVSM.hlsl"
#include "DescriptorTables.hlsl"

//=================================================================================================
// Constants
//=================================================================================================

#define ShadowMapMode_DepthMap_ 0
#define ShadowMapMode_EVSM2_     1
#define ShadowMapMode_EVSM4_     2

#ifndef ShadowMapMode_
	#define ShadowMapMode_ ShadowMapMode_DepthMap_
#endif

#if ShadowMapMode_ == ShadowMapMode_EVSM2_ || ShadowMapMode_ == ShadowMapMode_EVSM4_
	#define UseEVSM_ 1
#else
	#define UseEVSM_ 0
#endif

#ifndef MSAASamples_
	#define MSAASamples_ 1
#endif

#define MSAA_ (MSAASamples_ > 1)

#ifndef SampleRadius_
	#define SampleRadius_ 0
#endif

#if MSAA_
	#define MSAALoad_(tex, addr, subSampleIdx) tex.Load(uint2(addr), subSampleIdx)
#else
	#define MSAALoad_(tex, addr, subSampleIdx) tex[uint2(addr)]
#endif

//=================================================================================================
// Resources
//=================================================================================================
struct ConvertConstants
{
	float2 ShadowMapSize;
	float PositiveExponent;
	float NegativeExponent;

	float FilterSizeU;
	float FilterSizeV;

	uint InputMapIdx;
	uint ArraySliceIdx;
};

ConstantBuffer<ConvertConstants> CBuffer : register(b0);

SamplerState LinearSampler : register (s0);

//=================================================================================================
// ShadowMap Convert Pixel
//=================================================================================================
float4 SMConvert(in float4 PositionSS : SV_Position) : SV_Target0
{
	uint2 pixelPos = uint2(PositionSS.xy);

	// Convert to EVSM representation
#if MSAA_
	Texture2DMS<float4> ShadowMap = Tex2DMSTable[CBuffer.InputMapIdx];
#else
	Texture2D<float4> ShadowMap = Tex2DTable[CBuffer.InputMapIdx];
#endif

	float sampleWeight = 1.0f / float(MSAASamples_);

	float2 exponents = GetEVSMExponents(CBuffer.PositiveExponent, CBuffer.NegativeExponent, 1.0f);

	float4 average = 0.0f;

	// Sample indices to Load() must be literal, so force unroll
	[unroll]
	for (uint subSampleIdx = 0; subSampleIdx < MSAASamples_; ++subSampleIdx)
	{
		float depth = MSAALoad_(ShadowMap, pixelPos, subSampleIdx).x;

#if UseEVSM_
		float2 vsmDepth = WarpDepth(depth, exponents);
#else
		float2 vsmDepth = depth;
#endif
		average += sampleWeight * float4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);
	}

#if ShadowMapMode_ == ShadowMapMode_EVSM4_
	return average;
#else
	return average.xzxz;
#endif
}

//=================================================================================================
// Filter ShadowMap Pixel
//=================================================================================================
float4 FilterSM(in float4 Position : SV_Position) : SV_Target0
{
#if Vertical_
	const float filterSize = CBuffer.FilterSizeV;
	const float texelSize = rcp(CBuffer.ShadowMapSize.y);	
	Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];
#else
	const float filterSize = CBuffer.FilterSizeU;
	const float texelSize = rcp(CBuffer.ShadowMapSize.x);
	Texture2DArray<float4> shadowMap = Tex2DArrayTable[CBuffer.InputMapIdx];
#endif
	
	uint2 pixelPos = uint2(Position.xy);

	const float Radius = filterSize / 2.0f;
	const float2 TextureSize_ = CBuffer.ShadowMapSize;

	float4 sum = 0.0f;
	float weightSum = 0.0f;

#if 1 && SampleRadius_ > 0
	const float2 uv = Position.xy * rcp(TextureSize_);
	float edgeFraction = Radius - (SampleRadius_ - 0.5f);

	[unroll]
	for (float i = -SampleRadius_ + 0.5f; i <= SampleRadius_ + 0.5f; i += 2.0f)
	{
		float offset = i;
		float sampleWeight = 2.0f;

		if (i == -SampleRadius_ + 0.5f)
		{
			offset += (1.0f - edgeFraction) * 0.5f;
			sampleWeight = 1.0f + edgeFraction;
		}
		else if (i == SampleRadius_ + 0.5f)
		{
			offset -= 0.5f;
			sampleWeight = edgeFraction;
		}

#if Vertical_
		const float2 sampleUV = uv + float2(0.0f, offset) * texelSize;
		float4 smSample = shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f);
#else
		const float2 sampleUV = uv + float2(offset, 0.0f) * texelSize;
		float4 smSample = shadowMap.SampleLevel(LinearSampler, float3(sampleUV, CBuffer.ArraySliceIdx), 0.0f);
#endif
		//float4 smSample = shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f);

		sum += smSample * sampleWeight;
		weightSum += sampleWeight;
	}

	return sum / weightSum;
#else

	[unroll]
	for (int i = -SampleRadius_; i <= SampleRadius_; ++i)
	{
		float smWeight = saturate((Radius + 0.5f) - abs(i));

#if Vertical_
		float2 samplePos = pixelPos + float2(0.0f, i);
		samplePos = clamp(samplePos, 0, TextureSize_ - 1.0f);
		float4 smSample = shadowMap[uint2(samplePos)];
#else
		float2 samplePos = pixelPos + float2(i, 0.0f);
		samplePos = clamp(samplePos, 0, TextureSize_ - 1.0f);
		float4 smSample = shadowMap[uint3(samplePos, CBuffer.ArraySliceIdx)];
#endif

		sum += smSample * smWeight;
		weightSum += smWeight;
	}

	return sum / weightSum;
#endif
}

float4 FilterSM3x3(in float4 Position : SV_Position) : SV_Target0
{
	const float2 texelSize = rcp(CBuffer.ShadowMapSize);

	const float2 uv = Position.xy * texelSize;

	const float2 radius = float2(CBuffer.FilterSizeU, CBuffer.FilterSizeV) / 2.0f;
	float2 edgeFraction = radius - 0.5f;

	Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];

	float4 sum = 0.0f;

	// +---+---+---+
	// |       |   |
	// |       |   |
	// |       |   |
	// +   0   + 1 +
	// |       |   |
	// |       |   |
	// |       |   |
	// +---+---+---+
	// |       |   |
	// |   2   | 3 |
	// |       |   |
	// +---+---+---+

	{
		// Top left 2x2
		float2 offset = float2(-0.5f, -0.5f) + ((1.0f - edgeFraction) * float2(0.5f, 0.5f));
		float2 weights = 1.0f + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Top right 1x2
		float2 offset = float2(1.0f, -0.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
		float2 weights = float2(0.0f, 1.0f) + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Bottom left 2x1
		float2 offset = float2(-0.5f, 1.0f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
		float2 weights = float2(1.0f, 0.0f) + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Bottom right 1x1
		float2 offset = float2(1.0f, 1.0f);
		float2 weights = edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	return sum / (CBuffer.FilterSizeU * CBuffer.FilterSizeV);
}

float4 FilterSM5x5(in float4 Position : SV_Position) : SV_Target0
{
	const float2 texelSize = rcp(CBuffer.ShadowMapSize);

	const float2 uv = Position.xy * texelSize;

	const float2 radius = float2(CBuffer.FilterSizeU, CBuffer.FilterSizeV) / 2.0f;
	float2 edgeFraction = radius - 1.5f;

	Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];

	float4 sum = 0.0f;

	// +---+---+---+---+---+
	// |       |       |   |
	// |       |       |   |
	// |       |       |   |
	// +   0   +   1   + 2 +
	// |       |       |   |
	// |       |       |   |
	// |       |       |   |
	// +---+---+---+---+---+
	// |       |       |   |
	// |       |       |   |
	// |       |       |   |
	// +   3   +   4   + 5 +
	// |       |       |   |
	// |       |       |   |
	// |       |       |   |
	// +---+---+---+---+---+
	// |       |       |   |
	// |   6   |   7   | 8 |
	// |       |       |   |
	// +---+---+---+---+---+

	{
		// Top left 2x2
		float2 offset = float2(-1.5f, -1.5f) + ((1.0f - edgeFraction) * float2(0.5f, 0.5f));
		float2 weights = 1.0f + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Middle top 2x2
		float2 offset = float2(0.5f, -1.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
		float2 weights = float2(2.0f, 1.0f + edgeFraction.y);

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Top right 1x2
		float2 offset = float2(2.0f, -1.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
		float2 weights = float2(0.0f, 1.0f) + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Left middle 2x2
		float2 offset = float2(-1.5f, 0.5f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
		float2 weights = float2(1.0f + edgeFraction.x, 2.0f);

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Middle 2x2
		float2 offset = float2(0.5f, 0.5f);
		float2 weights = float2(2.0f, 2.0f);

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Middle right 1x2
		float2 offset = float2(2.0f, 0.5f);
		float2 weights = float2(edgeFraction.x, 2.0f);

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Bottom left 2x1
		float2 offset = float2(-1.5f, 2.0f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
		float2 weights = float2(1.0f, 0.0f) + edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Bottom middle 2x1
		float2 offset = float2(0.5f, 2.0f);
		float2 weights = float2(2.0f, edgeFraction.y);

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	{
		// Bottom right 1x1
		float2 offset = float2(2.0f, 2.0f);
		float2 weights = edgeFraction;

		const float2 sampleUV = uv + offset * texelSize;
		sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
	}

	return sum / (CBuffer.FilterSizeU * CBuffer.FilterSizeV);
}