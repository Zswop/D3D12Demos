#pragma once

#include "..\\PCH.h"
#include "..\\F12_Math.h"

namespace Framework
{
Float3 SampleDirectionCone(float u1, float u2, float cosThetaMax);
float SampleDirectionCone_PDF(float cosThetaMax);

Float3 SampleGGXMicrofacet(float roughness, float u1, float u2);
Float3 SampleDirectionGGX(const Float3& v, const Float3& n, float roughness, const Float3x3& tangentToWorld, float u1, float u2);
float SampleDirectionGGX_PDF(const Float3& n, const Float3& h, const Float3& v, float roughness);

Float2 Hammersley2D(uint64 sampleIdx, uint64 numSamples);
Float2 SampleCMJ2D(int32 sampleIdx, int32 numSamplesX, int32 numSamplesY, int32 pattern);

void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples);

float HaltonSeq(uint64 prime, uint64 index = 1 /* NOT! zero-based */);
void GenerateHaltonSamples2D(Float2* samples, uint64 numSamples);

}