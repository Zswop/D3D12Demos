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

static const char* FStopsLabels[23] =
{
	"f/1.8",
	"f/2.0",
	"f/2.2",
	"f/2.5",
	"f/2.8",
	"f/3.2",
	"f/3.5",
	"f/4.0",
	"f/4.5",
	"f/5.0",
	"f/5.6",
	"f/6.3",
	"f/7.1",
	"f/8.0",
	"f/9.0",
	"f/10.0",
	"f/11.0",
	"f/13.0",
	"f/14.0",
	"f/16.0",
	"f/18.0",
	"f/20.0",
	"f/22.0",
};

static const char* ISORatingsLabels[4] =
{
	"ISO100",
	"ISO200",
	"ISO400",
	"ISO800",
};

static const char* ShutterSpeedsLabels[13] =
{
	"1s",
	"1/2s",
	"1/4s",
	"1/8s",
	"1/15s",
	"1/30s",
	"1/60s",
	"1/125s",
	"1/250s",
	"1/500s",
	"1/1000s",
	"1/2000s",
	"1/4000s",
};


namespace AppSettings
{
	static SettingsContainer Settings;
	
	DirectionSetting SunDirection;
	ColorSetting SunTintColor;
	FloatSetting SunIntensityScale;
	ColorSetting GroundAlbedo;
	FloatSetting Turbidity;
	FloatSetting SunSize;
	SkyModesSetting SkyMode;

	BoolSetting RenderLights;
	ScenesSetting CurrentScene;

	MSAAModesSetting MSAAMode;
	FloatSetting ResolveFilterRadius;
	FloatSetting FilterGaussianSigma;
	ResolveFiltersSetting ResolveFilter;

	ExposureModesSetting ExposureMode;
	FloatSetting ManualExposure;
	FloatSetting ExposureFilterOffset;
	FStopsSetting ApertureSize;
	ISORatingsSetting ISORating;
	ShutterSpeedsSetting ShutterSpeed;
	
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
		Settings.Initialize(5);

		Settings.AddGroup("Scene", true);

		Settings.AddGroup("Camera Controls", false);

		Settings.AddGroup("Post Processing", false);

		Settings.AddGroup("Anti Aliasing", false);
		
		Settings.AddGroup("Debug", false);

		CurrentScene.Initialize("CurrentScene", "Scene", "Current Scene", "", Scenes::BoxTest, 2, ScenesLabels);
		Settings.AddSetting(&CurrentScene);

		SunDirection.Initialize("SunDirection", "Scene", "Sun Direction", "Direction of the sun", Float3(0.2600f, 0.9870f, -0.1600f), true);
		Settings.AddSetting(&SunDirection);

		SunTintColor.Initialize("SunTintColor", "Scene", "Sun Tint Color", "The color of the sun", Float3(1.0000f, 1.0000f, 1.0000f), false, -340282300000000000000000000000000000000.0000f, 340282300000000000000000000000000000000.0000f, 0.0100f, ColorUnit::None);
		Settings.AddSetting(&SunTintColor);

		SunIntensityScale.Initialize("SunIntensityScale", "Scene", "Sun Intensity Scale", "Scale the intensity of the sun", 1.0000f, 0.0000f, 340282300000000000000000000000000000000.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&SunIntensityScale);

		SunSize.Initialize("SunSize", "Scene", "Sun Size", "Angular radius of the sun in degrees", 0.2700f, 0.0100f, 340282300000000000000000000000000000000.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&SunSize);

		SkyMode.Initialize("SkyMode", "Scene", "SkyMode", "Controls the sky used for GI baking and background rendering", SkyModes::Procedural, 5, SkyModesLabels);
		Settings.AddSetting(&SkyMode);

		Turbidity.Initialize("Turbidity", "Scene", "Turbidity", "Atmospheric turbidity (thickness) uses for procedural sun and sky model", 2.0000f, 1.0000f, 10.0000f, 0.0100f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&Turbidity);

		GroundAlbedo.Initialize("GroundAlbedo", "Scene", "Ground Albedo", "Ground albedo color used for procedural sun and sky model", Float3(0.2500f, 0.2500f, 0.2500f), false, -340282300000000000000000000000000000000.0000f, 340282300000000000000000000000000000000.0000f, 0.0100f, ColorUnit::None);
		Settings.AddSetting(&GroundAlbedo);

		RenderLights.Initialize("RenderLights", "Scene", "Render Lights", "Enable or disable deferred light rendering", false);
		Settings.AddSetting(&RenderLights);

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

		ExposureMode.Initialize("ExposureMode", "Camera Controls", "Exposure Mode", "Specifies how exposure should be controled", ExposureModes::Automatic, 4, ExposureModesLabels);
		Settings.AddSetting(&ExposureMode);

		ManualExposure.Initialize("ManualExposure", "Camera Controls", "Exposure", "Simple exposure value applied to the scene before tone mapping (uses log2 scale)", -14.0000f, -24.0000f, 24.0000f, 0.1000f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&ManualExposure);

		ExposureFilterOffset.Initialize("ExposureFilterOffset", "Camera Controls", "Exposure Filter Offset", "", 2.0000f, -24.0000f, 24.0000f, 0.1000f, ConversionMode::None, 1.0000f);
		Settings.AddSetting(&ExposureFilterOffset);

		ApertureSize.Initialize( "ApertureSize", "Camera Controls", "Aperture", "", FStops::FStop16Point0, 23, FStopsLabels);
		Settings.AddSetting(&ApertureSize);

		ShutterSpeed.Initialize("ShutterSpeed", "Camera Controls", "Shutter Speed", "", ShutterSpeeds::ShutterSpeed1Over125, 13, ShutterSpeedsLabels);
		Settings.AddSetting(&ShutterSpeed);

		ISORating.Initialize("ISORating", "Camera Controls", "ISO Rating", "", ISORatings::ISO100, 4, ISORatingsLabels);
		Settings.AddSetting(&ISORating);

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
		ppSettings.ManualExposure = ManualExposure;
		ppSettings.ApertureFNumber = ApertureFNumber_();
		ppSettings.ShutterSpeedValue = ShutterSpeedValue_();
		ppSettings.ISO = ISO_();

		ppSettings.BloomExposure = BloomExposure;
		DX12::BindTempConstantBuffer(cmdList, ppSettings, rootParameter, CmdListMode::Graphics);
	}
}