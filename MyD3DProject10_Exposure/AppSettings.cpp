#include "PCH.h"
#include "AppSettings.h"

using namespace Framework;

static const char* SkyModesLabels[] =
{
	"None",
	"Procedural",
	"CubeMapEnnis",
	"CubeMapUffizi",
	"CubeMapGraceCathedral",
};

static const char* MSAAModesLabels[] =
{
	"None",
	"2x",
	"4x",
	"8x",
};

static const char* ScenesLabels[] =
{
	"Sponza",
	"BoxTest",
};

static const char* ResolveFilterLabels[] =
{
	"Box",
	"Triangle",
	"Gaussian",
	"BlackmanHarris",
	"Smoothstep",
	"BSpline",
	"CatmullRom",
	"Mitchell",
	"GeneralizedCubic",
	"Sinc",
};

static const char* JitterModesLabels[] =
{
	"None",
	"Jitter2x",
	"Jitter4x",
	"Jitter8x",
	"Jitter16x",
	"Jitter32x",
};

static const char* ExposureModesLabels[4] =
{
	"Manual (Simple)",
	"Manual (SBS)",
	"Manual (SOS)",
	"Automatic",
};

namespace AppSettings
{
	static SettingsContainer Settings;

	SkyModesSetting SkyMode;
	DirectionSetting SunDirection;
	ColorSetting GroundAlbedo;
	FloatSetting Turbidity;
	FloatSetting SunSize;

	ScenesSetting CurrentScene;

	MSAAModesSetting MSAAMode;
	FloatSetting ResolveFilterRadius;
	FloatSetting FilterGaussianSigma;
	ResolveFiltersSetting ResolveFilter;

	ExposureModesSetting ExposureMode;
	FloatSetting ManualExposure;
	FloatSetting ExposureFilterOffset;
	
	BoolSetting EnableTemporalAA;
	FloatSetting TemporalAABlendFactor;
	JitterModesSetting JitterMode;

	FloatSetting BloomExposure;
	FloatSetting BloomMagnitude;
	FloatSetting BloomBlurSigma;

	BoolSetting EnableRayTracing;
	BoolSetting ShowClusterVisualizer;

	const uint32 CBufferRegister = 12;
	ConstantBuffer CBuffer;

	void AppSettings::Initialize()
	{
		Settings.Initialize(4);

		Settings.AddGroup("Scene", true);

		Settings.AddGroup("Post Processing", false);

		Settings.AddGroup("Anti Aliasing", false);
		
		Settings.AddGroup("Debug", false);

		CurrentScene.Initialize("CurrentScene", "Scene", "Current Scene", "", Scenes::Sponza, 2, ScenesLabels);
		Settings.AddSetting(&CurrentScene);

		SunSize.Initialize("SunSize", "Scene", "Sun Size", "Angular radius of the sun in degrees", 1.0000f, 0.0100f, 340282300000000000000000000000000000000.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&SunSize);

		SunDirection.Initialize("SunDirection", "Scene", "Sun Direction", "Direction of the sun", Float3(0.2600f, 0.9870f, -0.1600f), true);
		Settings.AddSetting(&SunDirection);

		SkyMode.Initialize("SkyMode", "Scene", "SkyMode", "Controls the sky used for GI baking and background rendering", SkyModes::Procedural, 5, SkyModesLabels);
		Settings.AddSetting(&SkyMode);

		Turbidity.Initialize("Turbidity", "Scene", "Turbidity", "Atmospheric turbidity (thickness) uses for procedural sun and sky model", 2.0000f, 1.0000f, 10.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&Turbidity);

		GroundAlbedo.Initialize("GroundAlbedo", "Scene", "Ground Albedo", "Ground albedo color used for procedural sun and sky model", Float3(0.2500f, 0.2500f, 0.2500f), false, -340282300000000000000000000000000000000.0000f, 340282300000000000000000000000000000000.0000f, 0.0100f, ColorUnit::None);
		Settings.AddSetting(&GroundAlbedo);

		MSAAMode.Initialize("MSAAMode", "Anti Aliasing", "MSAA Mode", "MSAA mode to use for rendering", MSAAModes::MSAANone, 4, MSAAModesLabels);
		Settings.AddSetting(&MSAAMode);

		ResolveFilterRadius.Initialize("ResolveFilterRadius", "Anti Aliasing", "Resolve Filter Radius", "", 1.0000f, 0.0000f, 3.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&ResolveFilterRadius);

		FilterGaussianSigma.Initialize("FilterGaussianSigma", "Anti Aliasing", "Filter Gaussian Sigma", "", 0.5000f, 0.0000f, 3.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&FilterGaussianSigma);

		ResolveFilter.Initialize("ResolveFilter", "Anti Aliasing", "Resolve Filter", "", ResolveFilters::BSpline, 10, ResolveFilterLabels);
		Settings.AddSetting(&ResolveFilter);

		EnableTemporalAA.Initialize("EnableTemporalAA", "Anti Aliasing", "Enable TemporalAA", "", false);
		Settings.AddSetting(&EnableTemporalAA);

		TemporalAABlendFactor.Initialize("TemporalAABlendFactor", "Anti Aliasing", "TemporalAA Blend Factor", "", 0.1000f, 0.0000f, 1.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&TemporalAABlendFactor);

		JitterMode.Initialize("JitterMode", "Anti Aliasing", "Jitter Mode", "", JitterModes::Jitter2x, 6, JitterModesLabels);
		Settings.AddSetting(&JitterMode);

		ExposureMode.Initialize("ExposureMode", "Post Processing", "Exposure Mode", "Specifies how exposure should be controled", ExposureModes::Automatic, 4, ExposureModesLabels);
		Settings.AddSetting(&ExposureMode);

		ManualExposure.Initialize("ManualExposure", "Post Processing", "Exposure", "Simple exposure value applied to the scene before tone mapping (uses log2 scale)", -14.0000f, -24.0000f, 24.0000f, 0.1000f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&ManualExposure);

		ExposureFilterOffset.Initialize("ExposureFilterOffset", "Post Processing", "Exposure Filter Offset", "", 2.0000f, -24.0000f, 24.0000f, 0.1000f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&ExposureFilterOffset);

		BloomExposure.Initialize("BloomExposure", "Post Processing", "Bloom Exposure Offset", "Exposure offset applied to generate the input of the bloom pass", -4.0000f, -10.0000f, 0.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&BloomExposure);

		BloomMagnitude.Initialize("BloomMagnitude", "Post Processing", "Bloom Magnitude", "Scale factor applied to the bloom results when combined with tone-mapped result", 1.0000f, 0.0000f, 2.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&BloomMagnitude);

		BloomBlurSigma.Initialize("BloomBlurSigma", "Post Processing", "Bloom Blur Sigma", "Sigma parameter of the Gaussian filter used in the bloom pass", 2.5000f, 0.5000f, 2.5000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&BloomBlurSigma);

		EnableRayTracing.Initialize("EnableRayTracing", "Debug", "Enable RayTracing", "", false);
		Settings.AddSetting(&EnableRayTracing);

		ShowClusterVisualizer.Initialize("ShowClusterVisualizer", "Debug", "Show Cluster Visualizer", "Shows an overhead perspective of the view frustum with a visualization of the light/decal counts", false);
		Settings.AddSetting(&ShowClusterVisualizer);
	}

	void AppSettings::Shutdown()
	{
	}

	void AppSettings::Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix)
	{
		Settings.Update(displayWidth, displayHeight, viewMatrix);
	}

	void AppSettings::UpdateCBuffer()
	{
	}

	void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
	}

	void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
	}

	void BindPPCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
		PPSettings ppSettings;
		ppSettings.ExposureMode = ExposureMode;
		ppSettings.BloomExposure = BloomExposure;
		ppSettings.ManualExposure = ManualExposure;
		DX12::BindTempConstantBuffer(cmdList, ppSettings, rootParameter, CmdListMode::Graphics);
	}
}