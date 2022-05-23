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

	Scenes CurrentScene;
	bool ShowClusterVisualizer;

	MSAAModes MSAAMode;
	float ResolveFilterRadius;
	ResolveFilterTypes ResolveFilterType;
	float FilterGaussianSigma;

	bool32 EnableTemporalAA;
	JitterModes JitterMode;

	void AppSettings::Initialize()
	{
		SkyMode = SkyModes::Procedural;

		SunDirection = Float3(0.2600f, 0.9870f, -0.1600f);
		GroundAlbedo = Float3(0.2500f, 0.2500f, 0.2500f); 
		Turbidity = 2.0f;
		SunSize = 1.0f;

		CurrentScene = Scenes::Sponza;
		ShowClusterVisualizer = false;

		MSAAMode = MSAAModes::MSAA4x;

		ResolveFilterRadius = 1.0f;
		ResolveFilterType = ResolveFilterTypes::BSpline;
		FilterGaussianSigma = 0.5f;

		EnableTemporalAA = true;
		JitterMode = JitterModes::Jitter2x;
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