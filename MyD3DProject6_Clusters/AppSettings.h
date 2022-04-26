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

	NumValues
};

namespace AppSettings
{
	static const uint64 NumZTiles = 16;	
	static const uint64 ClusterTileSize = 16;
	static const uint64 MaxSpotLights = 32;
	static const uint64 SpotLightElementsPerCluster = 1;
	static const float SpotShadowNearClip = 0.1000f;
	static const float SpotLightRange = 7.5f;
	static const uint64 NumConeSides = 16;

	extern SkyModes SkyMode;
	extern Float3 SunDirection;
	extern Float3 GroundAlbedo;
	extern float Turbidity;
	extern float SunSize;

	void Initialize();
	void Shutdown();
	void Update(uint32 displayWidth, uint32 displayHeight);
	void UpdateCBuffer();
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
	};

	static const float SceneScales[] = { 0.01f };
	static const Float3 SceneCameraPositions[] = { Float3(-11.5f, 1.85f, -0.45f) };
	static const Float2 SceneCameraRotations[] = { Float2(0.0f, 1.544f) };

	StaticAssert_(ArraySize_(ScenePaths) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneScales) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneCameraPositions) == uint64(Scenes::NumValues));
	StaticAssert_(ArraySize_(SceneCameraRotations) == uint64(Scenes::NumValues));

}

namespace AppSettings
{
	extern uint64 NumXTiles;
	extern uint64 NumYTiles;
}