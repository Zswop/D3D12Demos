#include "Descriptortables.hlsl"
#include "Constants.hlsl"

//=================================================================================================
// Constant buffers
//=================================================================================================

struct SRVIndicesLayout
{
	uint Idx0;
	uint Idx1;
	uint Idx2;
	uint Idx3;
	uint Idx4;
	uint Idx5;
	uint Idx6;
	uint Idx7;
};

ConstantBuffer<SRVIndicesLayout> SRVIndices : register(b0);

//=================================================================================================
// Samplers
//=================================================================================================

SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);
SamplerState LinearWrapSampler : register(s2);
SamplerState LinearBorderSampler : register(s3);

//=================================================================================================
// Input/Output structs
//=================================================================================================

struct PSInput
{
	float4 PositionSS : SV_Position;
	float2 TexCoord : TEXCOORD;
};

//=================================================================================================
// Helper Functions
//=================================================================================================

// Applies the approximated version of HP Duiker's film stock curve
float3 ToneMapFilmicALU(in float3 color)
{
	color = max(0, color - 0.004f);
	color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
	return color;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================

// Uses hw bilinear filtering for upscaling or downscaling
float4 Scale(in PSInput input) : SV_Target
{
	Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];
	return inputTexture.Sample(LinearSampler, input.TexCoord);
}

// Applies exposure and tone mapping to the input
float4 ToneMap(in PSInput input) : SV_Target0
{
	Texture2D inputTexture0 = Tex2DTable[SRVIndices.Idx0];

	float3 color = inputTexture0.Sample(PointSampler, input.TexCoord).xyz;

	// Apply exposure (accounting for the FP16 scale used for lighting and emissive sources)
	//color *= exp2(-14.0f) / FP16Scale;

	// Tone map to sRGB color space with the appropriate transfer function applied
	//color = ToneMapFilmicALU(color);

	return float4(color, 1.0f);
}