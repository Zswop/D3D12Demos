#include "Descriptortables.hlsl"
#include "Constants.hlsl"
#include "ToneMapping.hlsl"
#include "Exposure.hlsl"

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

cbuffer PPSettings : register(b1)
{
	float BloomExposure;
	int ExposureMode;
	float ManualExposure;
	float ApertureFNumber;
	float ShutterSpeedValue;
	float ISO;
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
float4 UberPost(in PSInput input) : SV_Target0
{
	Texture2D inputTexture0 = Tex2DTable[SRVIndices.Idx0];
	Texture2D inputTexture1 = Tex2DTable[SRVIndices.Idx1];
	Texture2D inputTexture2 = Tex2DTable[SRVIndices.Idx2];

	// Retrieves the log-average luminance from the texture
	float avgLuminance = inputTexture2.Load(uint3(0, 0, 0)).x;
	float3 color = inputTexture0.Sample(PointSampler, input.TexCoord).xyz;

	// Add bloom
	color += inputTexture1.Sample(LinearSampler, input.TexCoord).xyz * exp2(BloomExposure);

	ExposureConstants expConstants;
	expConstants.ExposureMode = ExposureMode;
	expConstants.ManualExposure = ManualExposure;
	expConstants.AvgLuminance = avgLuminance;
	expConstants.ApertureFNumber = ApertureFNumber;
	expConstants.ShutterSpeedValue = ShutterSpeedValue;
	expConstants.ISO = ISO;

	float exposure = 0.0;
	color = CalcExposedColor(expConstants, color, exposure);

	color = ToneMap(color);

	return float4(color, 1.0f);
}