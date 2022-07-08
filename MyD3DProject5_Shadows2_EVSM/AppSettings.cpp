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

	float PositiveExponent;
	float NegativeExponent;
	float LightBleedingReduction;
	float VSMBias;

	void AppSettings::Initialize()
	{
		SkyMode = SkyModes::Procedural;

		//Float3(0.2600f, 0.9870f, -0.1600f) Float3::Normalize(Float3(1.0f, 1.0f, -1.0f))
		SunDirection = Float3(0.2600f, 0.9870f, -0.1600f);
		GroundAlbedo = Float3(0.2500f, 0.2500f, 0.2500f); 
		Turbidity = 2.0f;
		SunSize = 1.0f;

		PositiveExponent = 40.0f;
		NegativeExponent = 5.0f;
		LightBleedingReduction = 0.25f;
		VSMBias = 0.001f;
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