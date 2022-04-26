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

namespace AppSettings
{
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
}