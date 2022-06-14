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

	MSAAModes MSAAMode;
	float ResolveFilterRadius;
	ResolveFilterTypes ResolveFilterType;
	float FilterGaussianSigma;

	float ExposureFilterOffset;
	float Exposure;

	bool32 EnableTemporalAA;
	float TemporalAABlendFactor;
	JitterModes JitterMode;

	bool32 EnableRayTracing;
	bool32 ShowClusterVisualizer;

	const uint32 CBufferRegister = 12;
	ConstantBuffer CBuffer;	

	void AppSettings::Initialize()
	{
		SkyMode = SkyModes::Procedural;

		SunDirection = Float3(0.2600f, 0.9870f, -0.1600f);
		GroundAlbedo = Float3(0.2500f, 0.2500f, 0.2500f); 
		Turbidity = 2.0f;
		SunSize = 1.0f;

		CurrentScene = Scenes::Sponza;
		MSAAMode = MSAAModes::MSAA4x;

		ResolveFilterRadius = 1.0f;
		ResolveFilterType = ResolveFilterTypes::BSpline;
		FilterGaussianSigma = 0.5f;

		ExposureFilterOffset = 2.0f;
		Exposure = -13.0f;

		EnableTemporalAA = false;
		TemporalAABlendFactor = 0.1f;
		JitterMode = JitterModes::Jitter2x;

		EnableRayTracing = false;
		ShowClusterVisualizer = false;
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

	void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
	}

	void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
	}

	void BindPPCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
	{
		PPSettings ppSettings;
		ppSettings.Exposure = Exposure;
		ppSettings.BloomExposure = -4.0f;
		DX12::BindTempConstantBuffer(cmdList, ppSettings, rootParameter, CmdListMode::Graphics);
	}
}