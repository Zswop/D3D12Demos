#pragma once

#include "PCH.h"
#include "Graphics\\GraphicsTypes.h"
#include "F12_Math.h"
#include "Settings.h"

using namespace Framework;

enum class SkyModes
{
	None = 0,
	Procedural = 1,
	CubeMapEnnis = 2,
	CubeMapUffizi = 3,
	CubeMapGraceCathedral = 4,

	NumValues
};

typedef EnumSettingT<SkyModes> SkyModesSetting;

enum class Scenes
{
	Sponza = 0,
	BoxTest = 1,

	NumValues
};

typedef EnumSettingT<Scenes> ScenesSetting;

enum class MSAAModes
{
	MSAANone = 0,
	MSAA2x = 1,
	MSAA4x = 2,
	MSAA8x = 3,

	NumValues
};

typedef EnumSettingT<MSAAModes> MSAAModesSetting;

enum class ResolveFilters
{
	Box = 0,
	Triangle,
	Gaussian,

	BlackmanHarris,
	Smoothstep,

	BSpline,

	CatmullRom,
	Mitchell,

	GeneralizedCubic,
	Sinc,
};

typedef EnumSettingT<ResolveFilters> ResolveFiltersSetting;

enum class JitterModes
{
	None = 0,
	Jitter2x = 1,
	Jitter4x = 2,
	Jitter8x = 3,
	Jitter16x = 4,
	Jitter32x = 5,

	NumValues
};

typedef EnumSettingT<JitterModes> JitterModesSetting;

enum class ExposureModes
{
	ManualSimple = 0,
	Manual_SBS = 1,
	Manual_SOS = 2,
	Automatic = 3,

	NumValues
};

typedef EnumSettingT<ExposureModes> ExposureModesSetting;

enum class FStops
{
	FStop1Point8 = 0,
	FStop2Point0 = 1,
	FStop2Point2 = 2,
	FStop2Point5 = 3,
	FStop2Point8 = 4,
	FStop3Point2 = 5,
	FStop3Point5 = 6,
	FStop4Point0 = 7,
	FStop4Point5 = 8,
	FStop5Point0 = 9,
	FStop5Point6 = 10,
	FStop6Point3 = 11,
	FStop7Point1 = 12,
	FStop8Point0 = 13,
	FStop9Point0 = 14,
	FStop10Point0 = 15,
	FStop11Point0 = 16,
	FStop13Point0 = 17,
	FStop14Point0 = 18,
	FStop16Point0 = 19,
	FStop18Point0 = 20,
	FStop20Point0 = 21,
	FStop22Point0 = 22,

	NumValues
};

typedef EnumSettingT<FStops> FStopsSetting;

enum class ISORatings
{
	ISO100 = 0,
	ISO200 = 1,
	ISO400 = 2,
	ISO800 = 3,

	NumValues
};

typedef EnumSettingT<ISORatings> ISORatingsSetting;

enum class ShutterSpeeds
{
	ShutterSpeed1Over1 = 0,
	ShutterSpeed1Over2 = 1,
	ShutterSpeed1Over4 = 2,
	ShutterSpeed1Over8 = 3,
	ShutterSpeed1Over15 = 4,
	ShutterSpeed1Over30 = 5,
	ShutterSpeed1Over60 = 6,
	ShutterSpeed1Over125 = 7,
	ShutterSpeed1Over250 = 8,
	ShutterSpeed1Over500 = 9,
	ShutterSpeed1Over1000 = 10,
	ShutterSpeed1Over2000 = 11,
	ShutterSpeed1Over4000 = 12,

	NumValues
};

typedef EnumSettingT<ShutterSpeeds> ShutterSpeedsSetting;

namespace AppSettings
{
	extern DirectionSetting SunDirection;
	extern ColorSetting SunTintColor;
	extern FloatSetting SunIntensityScale;
	extern ColorSetting GroundAlbedo;
	extern FloatSetting Turbidity;
	extern FloatSetting SunSize;
	extern SkyModesSetting SkyMode;

	extern BoolSetting RenderLights;
	extern BoolSetting EnableDirectLighting;
	extern BoolSetting EnableIndirectLighting;
	extern FloatSetting NormalMapIntensity;
	extern FloatSetting RoughnessScale;

	extern ScenesSetting CurrentScene;

	extern MSAAModesSetting MSAAMode;
	extern FloatSetting ResolveFilterRadius;
	extern FloatSetting FilterGaussianSigma;
	extern ResolveFiltersSetting ResolveFilter;

	extern ExposureModesSetting ExposureMode;
	extern FloatSetting ExposureFilterOffset;
	extern FloatSetting ManualExposure;
	extern FStopsSetting ApertureSize;
	extern ISORatingsSetting ISORating;
	extern ShutterSpeedsSetting ShutterSpeed;

	extern FloatSetting BloomExposure;
	extern FloatSetting BloomMagnitude;
	extern FloatSetting BloomBlurSigma;

	extern BoolSetting EnableTemporalAA;
	extern FloatSetting TemporalAABlendFactor;
	extern JitterModesSetting JitterMode;

	extern BoolSetting EnableRayTracing;
	extern BoolSetting ShowClusterVisualizer;

	const extern uint32 CBufferRegister;
	extern ConstantBuffer CBuffer;

	void Initialize();
	void Shutdown();
	void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix);
	void UpdateCBuffer();

	void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
	void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
}

namespace AppSettings
{
	struct PPSettings
	{
		int ExposureMode;
		float ManualExposure;
		float ApertureFNumber;
		float ShutterSpeedValue;
		float ISO;

		float BloomExposure;
		float BloomMagnitude;
		float BloomBlurSigma;
	};

	inline float ISO_()
	{
		static const float ISOValues[] =
		{
			100.0f, 200.0f, 400.0f, 800.0f
		};
		StaticAssert_(ArraySize_(ISOValues) == uint64(ISORatings::NumValues));

		return ISOValues[uint64(AppSettings::ISORating)];
	}

	inline float ApertureFNumber_()
	{
		static const float FNumbers[] =
		{
			1.8f, 2.0f, 2.2f, 2.5f, 2.8f, 3.2f, 3.5f, 4.0f, 4.5f, 5.0f, 5.6f, 6.3f, 7.1f, 8.0f,
			9.0f, 10.0f, 11.0f, 13.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f
		};
		StaticAssert_(ArraySize_(FNumbers) == uint64(FStops::NumValues));

		return FNumbers[uint64(AppSettings::ApertureSize)];
	}

	inline float ShutterSpeedValue_()
	{
		static const float ShutterSpeedValues[] =
		{
			1.0f / 1.0f, 1.0f / 2.0f, 1.0f / 4.0f, 1.0f / 8.0f, 1.0f / 15.0f, 1.0f / 30.0f,
			1.0f / 60.0f, 1.0f / 125.0f, 1.0f / 250.0f, 1.0f / 500.0f, 1.0f / 1000.0f, 1.0f / 2000.0f, 1.0f / 4000.0f,
		};
		StaticAssert_(ArraySize_(ShutterSpeedValues) == uint64(ShutterSpeeds::NumValues));

		return ShutterSpeedValues[uint64(AppSettings::ShutterSpeed)];
	}

	void BindPPCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
}

namespace AppSettings
{
	const uint64 CubeMapStart = uint64(SkyModes::CubeMapEnnis);
	const uint64 NumCubeMaps = uint64(SkyModes::NumValues) - CubeMapStart;

	inline const wchar* CubeMapPaths(uint64 idx)
	{
		Assert_(idx < NumCubeMaps);

		const wchar* Paths[] =
		{
			L"Content\\EnvMaps\\Ennis.dds",
			L"Content\\EnvMaps\\Uffizi.dds",
			L"Content\\EnvMaps\\GraceCathedral.dds",
		};
		StaticAssert_(ArraySize_(Paths) == NumCubeMaps);

		return Paths[idx];
	}

	// Model filenames
	static const wchar* ScenePaths[] =
	{
		L"Content\\Models\\Sponza\\Sponza.fbx",
		nullptr,
	};

	static const float SceneScales[] = 
	{ 
		0.01f,
		1.0f
	};

	static const Float3 SceneCameraPositions[] = 
	{ 
		Float3(-11.5f, 1.85f, -0.45f), 
		Float3(0.5f, 2.5f, -6.0f)
	};

	static const Float2 SceneCameraRotations[] = 
	{ 
		Float2(0.0f, 1.544f), 
		Float2(0.0f, 0.0f) 
	};

	StaticAssert_(ArraySize_(ScenePaths) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneScales) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneCameraPositions) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneCameraRotations) == uint64(Scenes::NumValues));

}

const uint64 NumMSAAModes = uint64(MSAAModes::NumValues);

namespace AppSettings
{
	inline uint32 NumMSAASamples(MSAAModes mode)
	{
		static const uint32 NumSamples[] = { 1, 2, 4, 8 };
		StaticAssert_(ArraySize_(NumSamples) >= uint64(MSAAModes::NumValues));
		return NumSamples[uint32(mode)];
	}

	inline uint32 NumMSAASamples()
	{
		return NumMSAASamples(MSAAMode);
	}
}

const uint64 NumJitterModes = uint64(JitterModes::NumValues);

namespace AppSettings
{
	inline uint32 NumJitterSamples(JitterModes mode)
	{
		static const uint32 NumSamples[] = { 1, 2, 4, 8, 16, 32 };
		StaticAssert_(ArraySize_(NumSamples) >= uint64(JitterModes::NumValues));
		return NumSamples[uint32(mode)];
	}

	inline uint32 NumJitterSamples()
	{
		return NumJitterSamples(JitterMode);
	}
}
