//=================================================================================================
// Includes
//=================================================================================================
#include "DescriptorTables.hlsl"
#include "Constants.hlsl"

//=================================================================================================
// Constant buffers
//=================================================================================================

struct ResolveConstants
{
	int FilterType;
	int SampleRadius;
	float2 TextureSize;
	
	float FilterRadius;
	float FilterGaussianSigma;
	float Exposure;
	float ExposureFilterOffset;

	int EnableTemporalAA;
	float TemporalAABlendFactor;
};

struct SRVIndexConstants
{
	uint InputTextureIdx;
	uint VelocityBufferIdx;
	uint DepthBufferIdx;
	uint PrevFrameTextureIdx;
};

ConstantBuffer<ResolveConstants> CBuffer : register(b0);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b1);

SamplerState LinearSampler : register(s0);

//=================================================================================================
// Pixel Shader
//=================================================================================================

#ifndef MSAASamples_
	#define MSAASamples_ 1
#endif

#define MSAA_ (MSAASamples_ > 1)

static const int FilterTypes_Box = 0;
static const int FilterTypes_Triangle = 1;
static const int FilterTypes_Gaussian = 2;
static const int FilterTypes_BlackmanHarris = 3;
static const int FilterTypes_Smoothstep = 4;
static const int FilterTypes_BSpline = 5;
static const int FilterTypes_CatmullRom = 6;
static const int FilterTypes_Mitchell = 7;
static const int FilterTypes_GeneralizedCubic = 8;
static const int FilterTypes_Sinc = 9;

// These are the sub-sample locations for the 2x, 4x, and 8x standard multisample patterns.
// See the MSDN documentation for the D3D11_STANDARD_MULTISAMPLE_QUALITY_LEVELS enumeration.
#if MSAASamples_ == 8
	static const float2 SubSampleOffsets[8] = {
		float2(0.0625f, -0.1875f),
		float2(-0.0625f,  0.1875f),
		float2(0.3125f,  0.0625f),
		float2(-0.1875f, -0.3125f),
		float2(-0.3125f,  0.3125f),
		float2(-0.4375f, -0.0625f),
		float2(0.1875f,  0.4375f),
		float2(0.4375f, -0.4375f),
	};
#elif MSAASamples_ == 4
	static const float2 SubSampleOffsets[4] = {
			float2(-0.125f, -0.375f),
			float2(0.375f, -0.125f),
			float2(-0.375f,  0.125f),
			float2(0.125f,  0.375f),
	};
#elif MSAASamples_ == 2
	static const float2 SubSampleOffsets[2] = {
		float2(0.25f,  0.25f),
		float2(-0.25f, -0.25f),
	};
#else
	static const float2 SubSampleOffsets[1] = {
		float2(0.0f, 0.0f),
	};
#endif

#if MSAA_
	#define MSAALoad_(tex, addr, subSampleIdx) tex.Load(uint2(addr), subSampleIdx)
#else
	#define MSAALoad_(tex, addr, subSampleIdx) tex[uint2(addr)]
#endif

float FilterBox(in float x)
{
	return x <= 1.0f;
}

float FilterTriangle(in float x)
{
	return saturate(1.0f - x);
}

float FilterGaussian(in float x)
{
	const float sigma = CBuffer.FilterGaussianSigma;
	const float g = 1.0f / sqrt(2.0f * 3.14159f * sigma * sigma);
	return g * exp(-(x * x) / (2 * sigma * sigma));
}

float FilterCubic(in float x, in float B, in float C)
{
	float y = 0.0f;
	float x2 = x * x;
	float x3 = x * x * x;
	if (x < 1)
		y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
	else if (x <= 2)
		y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

	return y / 6.0f;
}

float FilterSinc(in float x, in float filterRadius)
{
	float s;

	x *= filterRadius * 2.0f;

	if (x < 0.001f)
		s = 1.0f;
	else
		s = sin(x * Pi) / (x * Pi);

	return s;
}

float FilterBlackmanHarris(in float x)
{
	x = 1.0f - x;

	const float a0 = 0.35875f;
	const float a1 = 0.48829f;
	const float a2 = 0.14128f;
	const float a3 = 0.01168f;
	return saturate(a0 - a1 * cos(Pi * x) + a2 * cos(2 * Pi * x) - a3 * cos(3 * Pi * x));
}

float FilterSmoothstep(in float x)
{
	return 1.0f - smoothstep(0.0f, 1.0f, x);
}

float Filter(in float x, in int filterType, in float filterRadius)
{
	// Cubic filters naturually work in a [-2, 2] domain. For the resolve case we
	// want to rescale the filter so that it works in [-1, 1] instead
	float cubicX = x * 2.0f;

	if (filterType == FilterTypes_Box)
		return FilterBox(x);
	else if (filterType == FilterTypes_Triangle)
		return FilterTriangle(x);
	else if (filterType == FilterTypes_Gaussian)
		return FilterGaussian(x);
	else if (filterType == FilterTypes_BlackmanHarris)
		return FilterBlackmanHarris(x);
	else if (filterType == FilterTypes_Smoothstep)
		return FilterSmoothstep(x);
	else if (filterType == FilterTypes_BSpline)
		return FilterCubic(cubicX, 1.0, 0.0f);
	else if (filterType == FilterTypes_CatmullRom)
		return FilterCubic(cubicX, 0, 0.5f);
	else if (filterType == FilterTypes_Mitchell)
		return FilterCubic(cubicX, 1 / 3.0f, 1 / 3.0f);
	//else if (filterType == FilterTypes_GeneralizedCubic)
	//	return FilterCubic(cubicX, CubicB, CubicC);
	else if (filterType == FilterTypes_Sinc)
		return FilterSinc(x, filterRadius);
	else
		return 1.0f;
}

float Luminance(in float3 clr)
{
	return dot(clr, float3(0.2126f, 0.7152f, 0.0722f));
}

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 prevSample, float3 avg)
{
#if 1
	// note: only clips towards aabb center (but fast!)
	float3 p_clip = 0.5 * (aabbMax + aabbMin);
	float3 e_clip = 0.5 * (aabbMax - aabbMin);

	float3 v_clip = prevSample - p_clip;
	float3 v_unit = v_clip.xyz / e_clip;
	float3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return prevSample;// point inside aabb

#else
	float3 r = prevSample - avg;
	float3 rmax = aabbMax - avg.xyz;
	float3 rmin = aabbMin - avg.xyz;

	const float eps = 0.000001f;

	if (r.x > rmax.x + eps)
		r *= (rmax.x / r.x);
	if (r.y > rmax.y + eps)
		r *= (rmax.y / r.y);
	if (r.z > rmax.z + eps)
		r *= (rmax.z / r.z);

	if (r.x < rmin.x - eps)
		r *= (rmin.x / r.x);
	if (r.y < rmin.y - eps)
		r *= (rmin.y / r.y);
	if (r.z < rmin.z - eps)
		r *= (rmin.z / r.z);

	return avg + r;
#endif
}

#if MSAA_
float3 Reproject(in Texture2DMS<float4> velocityBuffer, in Texture2DMS<float4> depthBuffer, in Texture2D prevFrameTexture, in float2 pixelPos)
#else
float3 Reproject(in Texture2D velocityBuffer, in Texture2D depthBuffer, in Texture2D prevFrameTexture, in float2 pixelPos)
#endif
{
	float2 velocity = 0.0f;

	// Dilate 3x3 nearest depth: GDC 2015 - Temporal Reprojection Anti-Aliasing In INSIDE
	{
		float closestDepth = 10.f;
		for (int vy = -1; vy <= 1; ++vy)
		{
			for (int vx = -1; vx <= 1; ++vx)
			{
				[unroll]
				for (uint vsIdx = 0; vsIdx < MSAASamples_; ++vsIdx)
				{
					float2 neighborVelocity = MSAALoad_(velocityBuffer, pixelPos + int2(vx, vy), vsIdx).xy;
					float neighborDepth = MSAALoad_(depthBuffer, pixelPos + int2(vx, vy), vsIdx).x;
					if (neighborDepth < closestDepth) 
					{
						velocity = neighborVelocity;
						closestDepth = neighborDepth;
					}
				}
			}
		}
	}

	velocity *= CBuffer.TextureSize;
	float2 reprojectedPos = pixelPos - velocity;
	float2 reprojectedUV = reprojectedPos / CBuffer.TextureSize;
	
	//return prevFrameTexture.SampleLevel(LinearSampler, reprojectedUV, 0.0f).xyz;
	return prevFrameTexture[int2(reprojectedPos)].xyz;

	// Reprojection filter
	{
		float3 sum = 0.0f;
		float totalWeight = 0.0f;

		const float ExposureFilterOffset = CBuffer.ExposureFilterOffset;
		const float Exposure = exp2(CBuffer.Exposure + ExposureFilterOffset) / FP16Scale;

		const int ReprojectionFilter = FilterTypes_CatmullRom;

		for (int ty = -1; ty <= 2; ++ty)
		{
			for (int tx = -1; tx <= 2; ++tx)
			{
				float2 samplePos = floor(reprojectedPos + float2(tx, ty)) + 0.5f;
				float3 reprojectedSample = prevFrameTexture[int2(samplePos)].xyz;

				float2 sampleDist = abs(samplePos - reprojectedPos);
				float filterWeight = Filter(sampleDist.x, ReprojectionFilter, 1.0f) *
					Filter(sampleDist.y, ReprojectionFilter, 1.0f);

				float sampleLum = Luminance(reprojectedSample);
				sampleLum *= Exposure;
				filterWeight *= 1.0f / (1.0f + sampleLum);

				sum += reprojectedSample * filterWeight;
				totalWeight += filterWeight;
			}
		}
		return max(sum / totalWeight, 0.0f);
	}
}

float4 ResolvePS(in float4 Position : SV_Position) : SV_Target0
{
#if MSAA_
	Texture2DMS<float4> InputTexture = Tex2DMSTable[SRVIndices.InputTextureIdx];
	Texture2DMS<float4> VelocityBuffer = Tex2DMSTable[SRVIndices.VelocityBufferIdx];
	Texture2DMS<float4> DepthBuffer = Tex2DMSTable[SRVIndices.DepthBufferIdx];
#else
	Texture2D InputTexture = Tex2DTable[SRVIndices.InputTextureIdx];
	Texture2D VelocityBuffer = Tex2DTable[SRVIndices.VelocityBufferIdx];
	Texture2D DepthBuffer = Tex2DTable[SRVIndices.DepthBufferIdx];
#endif

	Texture2D PrevFrameTexture = Tex2DTable[SRVIndices.PrevFrameTextureIdx];

	uint2 pixelPos = uint2(Position.xy);

	const float ExposureFilterOffset = CBuffer.ExposureFilterOffset;
	const float Exposure = exp2(CBuffer.Exposure + ExposureFilterOffset) / FP16Scale;

	float3 sum = 0.0f;
	float totalWeight = 0.0f;

	float3 clrMin = 99999999.0f;
	float3 clrMax = -99999999.0f;

	float3 m1 = 0.0f;
	float3 m2 = 0.0f;
	float mWeight = 0.0f;

#if MSAA_
	const int SampleRadius_ = CBuffer.SampleRadius;
#else
	const int SampleRadius_ = 1;
#endif

	const int FilterType_ = CBuffer.FilterType;
	const float FilterRadius_ = CBuffer.FilterRadius;
	const float2 TextureSize_ = CBuffer.TextureSize;
	
	for (int y = -SampleRadius_; y <= SampleRadius_; ++y)
	{
		for (int x = -SampleRadius_; x <= SampleRadius_; ++x)
		{
			float2 sampleOffset = float2(x, y);
			float2 samplePos = pixelPos + sampleOffset;
			samplePos = clamp(samplePos, 0, TextureSize_ - 1.0f);

			[unroll]
			for (uint subSampleIdx = 0; subSampleIdx < MSAASamples_; subSampleIdx++)
			{
				float2 subSampleOffset = SubSampleOffsets[subSampleIdx].xy;
				float2 sampleDist = abs(sampleOffset + subSampleOffset) / FilterRadius_;

				#if MSAA_
					bool useSample = all(sampleDist <= 1.0f);
				#else
					bool useSample = true;
				#endif

				if (useSample)
				{
					float3 texSample = MSAALoad_(InputTexture, samplePos, subSampleIdx).xyz;
					texSample = max(texSample, 0.0f);

					float weight = Filter(sampleDist.x, FilterType_, FilterRadius_) *
						Filter(sampleDist.y, FilterType_, FilterRadius_);

					clrMin = min(clrMin, texSample);
					clrMax = max(clrMax, texSample);

					float sampleLum = Luminance(texSample);
					sampleLum *= Exposure;
					weight *= 1.0f / (1.0f + sampleLum);

					sum += texSample * weight;
					totalWeight += weight;

					m1 += texSample;
					m2 += texSample * texSample;
					mWeight += 1.0f;
				}
			}
		}
	}

#if MSAA_
	float3 output = sum / max(totalWeight, 0.00001f);
#else
	float3 output = InputTexture[uint2(pixelPos)].xyz;
#endif

	output = max(output, 0.0f);

	if (CBuffer.EnableTemporalAA)
	{
		float3 currColor = output;
		float3 prevColor = Reproject(VelocityBuffer, DepthBuffer, PrevFrameTexture, pixelPos);

		// Variance clipping: GDC 2016 - An excursion in temporal super sampling
		{
			float3 mu = m1 / mWeight;
			float3 sigma = sqrt(abs(m2 / mWeight - mu * mu));

			const float VarianceClipGamma = 1.0f;
			float3 minc = mu - VarianceClipGamma * sigma;
			float3 maxc = mu + VarianceClipGamma * sigma;

			prevColor = ClipAABB(minc, maxc, prevColor, mu);
		}

		const float TemporalAABlendFactor = 0.1f;
		float3 weightA = saturate(TemporalAABlendFactor);
		float3 weightB = saturate(1.0f - TemporalAABlendFactor);

		weightA *= 1.0f / (1.0f + Luminance(currColor));
		weightB *= 1.0f / (1.0f + Luminance(prevColor));
	
		output = (currColor * weightA + prevColor * weightB) / (weightA + weightB);
	}

	return float4(output, 1.0f);
}