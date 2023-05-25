#include "Constants.hlsl"
#include "ACES.hlsl"

// == Utility functions ===========================================================================

static const int ToneMappingModes_FilmStock = 0;
static const int ToneMappingModes_Linear = 1;
static const int ToneMappingModes_ACES = 2;
static const int ToneMappingModes_Hejl2015 = 3;
static const int ToneMappingModes_Hable = 4;

static const int ToneMappingMode = ToneMappingModes_ACES;

// Approximates luminance from an RGB value
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 LinearTosRGB(in float3 color)
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}

float3 SRGBToLinear(in float3 color)
{
    float3 x = color / 12.92f;
    float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

    float3 clr = color;
    clr.r = color.r <= 0.04045f ? x.r : y.r;
    clr.g = color.g <= 0.04045f ? x.g : y.g;
    clr.b = color.b <= 0.04045f ? x.b : y.b;

    return clr;
}

// Applies the filmic curve from John Hable's presentation
float3 ToneMapFilmicALU(in float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
    return color;
}

float3 ToneMap_Hejl2015(in float3 hdr)
{
    const float WhitePoint_Hejl = 1.0;
    float4 vh = float4(hdr, WhitePoint_Hejl);
    float4 va = (1.435f * vh) + 0.05;
    float4 vf = ((vh * va + 0.004f) / ((vh * (va + 0.55f) + 0.0491f))) - 0.0821f;
    return LinearTosRGB(vf.xyz / vf.www);
}

float3 NeutralCurve(float3 x, float a, float b, float c, float d, float e, float f)
{
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float3 ToneMap_Hable(in float3 color) {
    const float A = 4.0;
    const float B = 5.0;
    const float C = 0.12;
    const float D = 13.0;

    // Not exposed as settings
    const float E = 0.01f;
    const float F = 0.3f;

    const float WhitePoint_Hable = 6.0;

    float3 numerator = NeutralCurve(color, A, B, C, D, E, F);
    float3 denominator = NeutralCurve(WhitePoint_Hable, A, B, C, D, E, F);

    return LinearTosRGB(numerator / denominator);
}

float3 ToneMap_Neutral(in float3 x)
{
    // Tonemap
    const float a = 0.2;
    const float b = 0.29;
    const float c = 0.24;
    const float d = 0.272;
    const float e = 0.02;
    const float f = 0.3;
    const float whiteLevel = 5.3;

    float3 whiteScale = (1.0).xxx / NeutralCurve(whiteLevel, a, b, c, d, e, f);
    x = NeutralCurve(x * whiteScale, a, b, c, d, e, f);
    x *= whiteScale;

    return LinearTosRGB(x);
}

float3 ToneMap_ACES(in float3 srgb)
{
    float3 oces = RRT(sRGB_to_ACES(srgb));
    float3 odt = ODT_RGBmonitor_100nits_dim(oces);
    return odt;
}

// Applies exposure and tone mapping to the specific color, and applies
// the threshold to the exposure value.
float3 ToneMap(in float3 color)
{
    float3 output = 0;
    if (ToneMappingMode == ToneMappingModes_Linear)
        output = LinearTosRGB(color);
    else if (ToneMappingMode == ToneMappingModes_FilmStock)
        output = ToneMapFilmicALU(color);
    else if (ToneMappingMode == ToneMappingModes_ACES)
        output = ToneMap_ACES(color);
    else if (ToneMappingMode == ToneMappingModes_Hejl2015)
        output = ToneMap_Hejl2015(color);
    else if (ToneMappingMode == ToneMappingModes_Hable)
        output = ToneMap_Hable(color);

    return output;
}