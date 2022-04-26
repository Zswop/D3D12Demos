#include "PCH.h"
#include "AppSettings.h"

using namespace Framework;

namespace AppSettings
{
	SkyModes SkyMode;
	Float3 SunDirection;
	Float3 GroundAlbedo;
	float Turbidity;
	float SunSize;

	void AppSettings::Initialize()
	{
		SkyMode = SkyModes::Procedural;

		//Float3(0.2600f, 0.9870f, -0.1600f) Float3::Normalize(Float3(1.0f, 1.0f, -1.0f))
		SunDirection = Float3(0.2600f, 0.9870f, -0.1600f);
		GroundAlbedo = Float3(0.2500f, 0.2500f, 0.2500f); 
		Turbidity = 2.0f;
		SunSize = 1.0f;
	}

	void AppSettings::Shutdown()
	{
	}

	void AppSettings::Update(uint32 displayWidth, uint32 displayHeight)
	{
	}

	void AppSettings::UpdateCBuffer()
	{
	}
}

namespace AppSettings
{
	uint64 NumXTiles = 0;
	uint64 NumYTiles = 0;
}