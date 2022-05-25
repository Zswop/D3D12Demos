#include "Descriptortables.hlsl"
#include "Constants.hlsl"
#include "ACES.hlsl"

#include "AppSettings.hlsl"

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

struct PPSettingsLayout
{
	float Exposure;
	float BloomExposure;
};

ConstantBuffer<SRVIndicesLayout> SRVIndices : register(b0);
ConstantBuffer<PPSettingsLayout> PPSettings : register(b1);

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

// Calculates the gaussian blur weight for a given distance and sigmas
float CalcGaussianWeight(int sampleDist, float sigma)
{
	float g = 1.0f / sqrt(2.0f * 3.14159 * sigma * sigma);
	return (g * exp(-(sampleDist * sampleDist) / (2 * sigma * sigma)));
}

// Performs a gaussian blur in one direction
float4 Blur(in PSInput input, float2 texScale, float sigma, bool normalize)
{
	Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];

	float2 inputSize = 0.0f;
	inputTexture.GetDimensions(inputSize.x, inputSize.y);

	float4 color = 0;
	float weightSum = 0.0f;
	for (int i = -7; i <= 7; i++)
	{
		float weight = CalcGaussianWeight(i, sigma);
		weightSum += weight;

		float2 texcoord = input.TexCoord;
		texcoord += (i / inputSize) * texScale;

		float4 texSample = inputTexture.Sample(PointSampler, texcoord);

		color += texSample * weight;
	}

	if (normalize)
		color /= weightSum;

	return color;
}

float3 Linear2sRGB(float3 x)
{
	return (x <= 0.0031308 ? (x * 12.9232102) : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055);
}

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
	
	x = Linear2sRGB(x);
	return x;
}

float3 ToneMapACES(float3 srgb)
{	
	float3 oces = RRT(sRGB_to_ACES(srgb));
	float3 odt = ODT_RGBmonitor_100nits_dim(oces);
	
	return odt;
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

float4 Bloom(in PSInput input) : SV_Target
{
	Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];

	float4 reds = inputTexture.GatherRed(LinearSampler, input.TexCoord);
	float4 greens = inputTexture.GatherGreen(LinearSampler, input.TexCoord);
	float4 blues = inputTexture.GatherBlue(LinearSampler, input.TexCoord);

	float3 result = 0.0f;
	
	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		float3 color = float3(reds[i], greens[i], blues[i]);

		result += color;
	}

	result /= 4.0f;

	return float4(result, 1.0f);
}

float4 BlurH(in PSInput input) : SV_Target
{
	return Blur(input, float2(1, 0), 2.5f, false);
}

float4 BlurV(in PSInput input) : SV_Target
{
	return Blur(input, float2(0, 1), 2.5f, false);
}

// Applies exposure and tone mapping to the input
float4 ToneMap(in PSInput input) : SV_Target0
{
	Texture2D inputTexture0 = Tex2DTable[SRVIndices.Idx0];
	Texture2D inputTexture1 = Tex2DTable[SRVIndices.Idx1];

	float3 color = inputTexture0.Sample(PointSampler, input.TexCoord).xyz;

	// Add bloom
	color += inputTexture1.Sample(LinearSampler, input.TexCoord).xyz * exp2(PPSettings.BloomExposure);

	// Apply exposure (accounting for the FP16 scale used for lighting and emissive sources)
	color *= exp2(PPSettings.Exposure) / FP16Scale;

	// Tone map to sRGB color space with the appropriate transfer function applied
	//color = ToneMapFilmicALU(color);
	//color = ToneMapNeutral(color);
	color = ToneMapACES(color);

	return float4(color, 1.0f);
}