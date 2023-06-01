#include <DescriptorTables.hlsl>

//=================================================================================================
// Constants
//=================================================================================================
static const uint ReductionTGSize = 16;

static const uint NumThreads = ReductionTGSize * ReductionTGSize;

cbuffer PPConstants : register(b0)
{
    uint InputTextureIdx;
};

//=================================================================================================
// Resources
//=================================================================================================
RWTexture2D<float> OutputMap : register(u0);

// -- shared memory
groupshared float LumSamples[NumThreads];

// Calculates luminance from an RGB value
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

//=================================================================================================
// Luminance reduction, intial pass
//=================================================================================================
[numthreads(ReductionTGSize, ReductionTGSize, 1)]
void LuminanceReductionInitialCS(in uint3 GroupID : SV_GroupID,
    in uint3 GroupThreadID : SV_GroupThreadID,
    uint ThreadIndex : SV_GroupIndex)
{
    Texture2D inputTexture = Tex2DTable[InputTextureIdx];

    uint2 textureSize;
    inputTexture.GetDimensions(textureSize.x, textureSize.y);

    uint2 samplePos = GroupID.xy * ReductionTGSize + GroupThreadID.xy;
    samplePos = min(samplePos, textureSize - 1);

    float3 colorSample = inputTexture[samplePos].xyz;

    float lumSample = max(CalcLuminance(colorSample), 0.00001f);
    float pixelLuminance = log(lumSample);

    // -- store in shared memory
    LumSamples[ThreadIndex] = pixelLuminance;
    GroupMemoryBarrierWithGroupSync();

    // -- reduce
    [unroll]
    for (uint s = NumThreads / 2; s > 0; s >>= 1)
    {
        if (ThreadIndex < s)
            LumSamples[ThreadIndex] += LumSamples[ThreadIndex + s];

        GroupMemoryBarrierWithGroupSync();
    }

    if (ThreadIndex == 0)
        OutputMap[GroupID.xy] = LumSamples[0] / NumThreads;
}

//=================================================================================================
// Luminance reduction, 2nd pass onwards
//=================================================================================================
[numthreads(ReductionTGSize, ReductionTGSize, 1)]
void LuminanceReductionCS(in uint3 GroupID : SV_GroupID, in uint3 GroupThreadID : SV_GroupThreadID,
    in uint ThreadIndex : SV_GroupIndex)
{
    Texture2D inputTexture = Tex2DTable[InputTextureIdx];

    uint2 textureSize;
    inputTexture.GetDimensions(textureSize.x, textureSize.y);

    uint2 samplePos = GroupID.xy * ReductionTGSize + GroupThreadID.xy;
    samplePos = min(samplePos, textureSize - 1);

    float pixelLuminance = inputTexture[samplePos].x;

    // Store in shared memory
    LumSamples[ThreadIndex] = pixelLuminance;
    GroupMemoryBarrierWithGroupSync();

    // Reduce
    [unroll]
    for (uint s = NumThreads / 2; s > 0; s >>= 1)
    {
        if (ThreadIndex < s)
            LumSamples[ThreadIndex] += LumSamples[ThreadIndex + s];

        GroupMemoryBarrierWithGroupSync();
    }

    if (ThreadIndex == 0)
    {
#if FinalPass_
        // Perform adaptation
        float lastLum = OutputMap[uint2(0, 0)];
        float currentLum = exp(LumSamples[0] / NumThreads);

        // Adapt the luminance using Pattanaik's technique
        const bool EnableAdaptation = true;
        const float TimeDelta = 0.0167f;
        const float Tau = 0.5;
        float adaptedLum = EnableAdaptation ? lastLum + (currentLum - lastLum) * (1 - exp(-TimeDelta * Tau))
            : currentLum;
        OutputMap[uint2(0, 0)] = adaptedLum;
#else
        OutputMap[GroupID.xy] = LumSamples[0] / NumThreads;
#endif
    }
}