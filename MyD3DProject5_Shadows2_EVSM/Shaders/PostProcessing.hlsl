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

float3 NeutralCurve(float3 x, float a, float b, float c, float d, float e, float f)
{
	return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float3 ToneMapNeutral(in float3 x)
{
	// Tonemap
	const float a = 0.2;
	const float b = 0.29;
	const float c = 0.24;
	const float d = 0.272;
	const float e = 0.02;
	const float f = 0.3;
	const float whiteLevel = 5.3;
	const float whiteClip = 1.0;

	float3 whiteScale = (1.0).xxx / NeutralCurve(whiteLevel, a, b, c, d, e, f);
	x = NeutralCurve(x * whiteScale, a, b, c, d, e, f);
	x *= whiteScale;

	// Post-curve white point adjustment
	x /= whiteClip.xxx;

	return x;
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
	color *= exp2(-14.0f) / FP16Scale;

	// Tone map to sRGB color space with the appropriate transfer function applied
	color = ToneMapNeutral(color);

	return float4(color, 1.0f);
}