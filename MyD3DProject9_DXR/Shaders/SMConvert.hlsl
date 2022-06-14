//=================================================================================================
// Includes
//=================================================================================================
#include "EVSM.hlsl"
#include "DescriptorTables.hlsl"

//=================================================================================================
// Constants
//=================================================================================================
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
	float NearClip;
	float InvClipRange;

	float Proj33;
	float Proj43;

	uint InputMapIdx;
	uint ArraySliceIdx;
};

ConstantBuffer<ConvertConstants> CBuffer : register(b0);

SamplerState LinearSampler : register (s0);

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

		// Linearize depth
		depth = CBuffer.Proj43 / (depth - CBuffer.Proj33);
		depth = (depth - CBuffer.NearClip) * CBuffer.InvClipRange;

		float2 vsmDepth = WarpDepth(depth, exponents);
		average += sampleWeight * float4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);
	}
	return average;
}