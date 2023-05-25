#include "Sampling.h"

namespace Framework
{

Float3 SampleDirectionCone(float u1, float u2, float cosThetaMax)
{
	float cosTheta = (1.0f - u1) + u1 * cosThetaMax;
	float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
	float phi = u2 * 2.0f * Pi;
	return Float3(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
}

// Returns the PDF of of a single uniform sample within a cone
float SampleDirectionCone_PDF(float cosThetaMax)
{
	return 1.0f / (2.0f * Pi * (1.0f - cosThetaMax));
}

// Returns a microfacet normal (half direction) that can be be used to compute a
// reflected lighting direction.
Float3 SampleGGXMicrofacet(float roughness, float u1, float u2)
{
	float theta = std::atan2(roughness * std::sqrt(u1), std::sqrt(1 - u1));
	float phi = 2 * Pi * u2;

	Float3 h;
	h.x = std::sin(theta) * std::cos(phi);
	h.y = std::sin(theta) * std::sin(phi);
	h.z = std::cos(theta);

	return h;
}

// Returns a random direction for sampling a GGX distribution.
// Does everything in world space.
Float3 SampleDirectionGGX(const Float3& v, const Float3& n, float roughness, const Float3x3& tangentToWorld, float u1, float u2)
{
	Float3 h = SampleGGXMicrofacet(roughness, u1, u2);

	h = Float3::Normalize(Float3::Transform(h, tangentToWorld));

	float hDotV = std::abs(Float3::Dot(h, v));
	Float3 sampleDir = 2.0f * hDotV * h - v;
	return Float3::Normalize(sampleDir);
}

// Returns the PDF for a particular GGX sample, The PDF is equal to D(m) * dot(n, m) / (4 *  dot(v, m))
float SampleDirectionGGX_PDF(const Float3& n, const Float3& h, const Float3& v, float roughness)
{
	float nDotH = Saturate(Float3::Dot(n, h));
	float hDotV = Saturate(Float3::Dot(h, v));
	float m2 = roughness * roughness;
	float d = m2 / (Pi * Square(nDotH * nDotH * (m2 - 1) + 1));
	float pM = d * nDotH;
	return pM / (4 * hDotV);
}

// Computes a radical inverse with base 2 using crazy bit-twiddling from "Hacker's Delight"
float RadicalInverseBase2(uint32 bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// Returns a single 2D point in a Hammersley sequence of length "numSamples", using base 1 and base 2
Float2 Hammersley2D(uint64 sampleIdx, uint64 numSamples)
{
	return Float2(float(sampleIdx) / float(numSamples), RadicalInverseBase2(uint32(sampleIdx)));
}

static uint32 CMJPermute(uint32 i, uint32 l, uint32 p)
{
	uint32 w = l - 1;
	w |= w >> 1;
	w |= w >> 2;
	w |= w >> 4;
	w |= w >> 8;
	w |= w >> 16;
	do
	{
		i ^= p; i *= 0xe170893d;
		i ^= p >> 16;
		i ^= (i & w) >> 4;
		i ^= p >> 8; i *= 0x0929eb3f;
		i ^= p >> 23;
		i ^= (i & w) >> 1; i *= 1 | p >> 27;
		i *= 0x6935fa69;
		i ^= (i & w) >> 11; i *= 0x74dcb303;
		i ^= (i & w) >> 2; i *= 0x9e501cc3;
		i ^= (i & w) >> 2; i *= 0xc860a3df;
		i &= w;
		i ^= i >> 5;
	} while (i >= l);
	return (i + p) % l;
}

static float CMJRandFloat(uint32 i, uint32 p)
{
	i ^= p;
	i ^= i >> 17;
	i ^= i >> 10; i *= 0xb36534e5;
	i ^= i >> 12;
	i ^= i >> 21; i *= 0x93fc4795;
	i ^= 0xdf6e307f;
	i ^= i >> 17; i *= 1 | p >> 18;
	return i * (1.0f / 4294967808.0f);
}

// Returns a 2D sample from a particular pattern using correlated multi-jittered sampling [Kensler 2013]
Float2 SampleCMJ2D(int32 sampleIdx, int32 numSamplesX, int32 numSamplesY, int32 pattern)
{
	int32 N = numSamplesX * numSamplesY;
	sampleIdx = CMJPermute(sampleIdx, N, pattern * 0x51633e2d);
	int32 sx = CMJPermute(sampleIdx % numSamplesX, numSamplesX, pattern * 0x68bc21eb);
	int32 sy = CMJPermute(sampleIdx / numSamplesX, numSamplesY, pattern * 0x02e5be93);
	float jx = CMJRandFloat(sampleIdx, pattern * 0x967a889b);
	float jy = CMJRandFloat(sampleIdx, pattern * 0x368cc8b7);
	return Float2((sx + (sy + jx) / numSamplesY) / numSamplesX, (sampleIdx + jy) / N);
}

// Generates hammersley using base 1 and 2
void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples)
{
	for (uint64 i = 0; i < numSamples; ++i)
		samples[i] = Hammersley2D(i, numSamples);
}

float HaltonSeq(uint64 prime, uint64 index)
{
	float r = 0.0f;
	float f = 1.0f;
	uint64 i = index;
	while (i > 0)
	{
		f /= prime;
		r += f * (i % prime);
		i = (uint64)std::floor(i / (float)prime);
	}
	return r;
}

void GenerateHaltonSamples2D(Float2* samples, uint64 numSamples)
{
	for (uint64 i = 0; i < numSamples; i++)
	{
		float u = HaltonSeq(2, i + 1);
		float v = HaltonSeq(3, i + 1);
		samples[i] = Float2(u, v);
	}
}
}