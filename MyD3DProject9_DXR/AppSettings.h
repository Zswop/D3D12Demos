#pragma once

#include "PCH.h"
#include "Graphics\\GraphicsTypes.h"
#include "F12_Math.h"

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

enum class Scenes
{
	Sponza = 0,
	BoxTest = 1,

	NumValues
};

enum class MSAAModes
{
	MSAANone = 0,
	MSAA2x = 1,
	MSAA4x = 2,
	MSAA8x = 3,

	NumValues
};

enum class ResolveFilterTypes
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

namespace AppSettings
{
	extern SkyModes SkyMode;
	extern Float3 SunDirection;
	extern Float3 GroundAlbedo;
	extern float Turbidity;
	extern float SunSize;

	extern Scenes CurrentScene;

	extern MSAAModes MSAAMode;
	extern float ResolveFilterRadius;
	extern ResolveFilterTypes ResolveFilterType;
	extern float FilterGaussianSigma;

	extern float ExposureFilterOffset;
	extern float Exposure;

	extern bool32 EnableTemporalAA;
	extern float TemporalAABlendFactor;
	extern JitterModes JitterMode;

	extern bool32 EnableRayTracing;

	extern bool32 ShowClusterVisualizer;

	const extern uint32 CBufferRegister;
	extern ConstantBuffer CBuffer;

	void Initialize();
	void Shutdown();
	void Update(uint32 displayWidth, uint32 displayHeight);
	void UpdateCBuffer();
	
	void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
	void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
}

namespace AppSettings
{
	struct PPSettings
	{
		float Exposure;
		float BloomExposure;
	};

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
		Float3(0.0f, 2.5f, -10.0f)
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
