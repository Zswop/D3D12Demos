// The two functions below were based on code and explanations provided by Padraic Hennessy (@PadraicHennessy).
// See this for more info: https://placeholderart.wordpress.com/2014/11/21/implementing-a-physically-based-camera-manual-exposure/

static const int ExposureModes_ManualSimple = 0;
static const int ExposureModes_Manual_SBS = 1;
static const int ExposureModes_Manual_SOS = 2;
static const int ExposureModes_Automatic = 3;

struct ExposureConstants
{
    int ExposureMode;
    float AvgLuminance;
    float ManualExposure;
    float ApertureFNumber;
    float ShutterSpeedValue;
    float ISO;
};

float SaturationBasedExposure(in float ApertureFNumber, in float ShutterSpeedValue, in float ISO)
{
    float maxLuminance = (7800.0f / 65.0f) * (ApertureFNumber * ApertureFNumber) / (ISO * ShutterSpeedValue);
    return log2(1.0f / maxLuminance);
}

float StandardOutputBasedExposure(in float ApertureFNumber, in float ShutterSpeedValue, in float ISO, in float middleGrey = 0.18f)
{
    float lAvg = (1000.0f / 65.0f) * (ApertureFNumber * ApertureFNumber) / (ISO * ShutterSpeedValue);
    return log2(middleGrey / lAvg);
}

float Log2Exposure(in ExposureConstants expConstants)
{
    float exposure = 0.0f;

    const int ExposureMode = expConstants.ExposureMode;

    if (ExposureMode == ExposureModes_Automatic)
    {   
        const float KeyValue = 0.115f;

        float avgLuminance = expConstants.AvgLuminance;
        avgLuminance = max(avgLuminance, 0.00001f);
        float linearExposure = (KeyValue / avgLuminance);
        exposure = log2(max(linearExposure, 0.00001f));
    }
    else if (ExposureMode == ExposureModes_Manual_SBS)
    {
        exposure = SaturationBasedExposure(expConstants.ApertureFNumber, 
            expConstants.ShutterSpeedValue, expConstants.ISO);
        exposure -= log2(FP16Scale);
    }
    else if (ExposureMode == ExposureModes_Manual_SOS)
    {
        exposure = StandardOutputBasedExposure(expConstants.ApertureFNumber,
            expConstants.ShutterSpeedValue, expConstants.ISO);
        exposure -= log2(FP16Scale);
    }
    else
    {
        exposure = expConstants.ManualExposure;
        exposure -= log2(FP16Scale);
    }

    return exposure;
}

// Determines the color based on exposure settings
float3 CalcExposedColor(in ExposureConstants expConstants, in float3 color, in float offset, out float exposure)
{
    exposure = Log2Exposure(expConstants);
    exposure += offset;
    return exp2(exposure) * color;
}