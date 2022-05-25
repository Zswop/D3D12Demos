#ifndef SHAREDTYPES_HLSL_
#define SHAREDTYPES_HLSL_

static const uint MaxSpotLights = 32;

struct MaterialTextureIndices
{
	uint Albedo;
	uint Normal;
	uint Roughness;
	uint Metallic;
	uint Opacity;
	uint Emissive;
};

struct SpotLight
{
	float3 Position;
	float AngularAttenuationX;
	float3 Direction;
	float AngularAttenuationY;
	float3 Intensity;
	float Range;
};

struct ShadingConstants
{
	float3 SunDirectionWS;
	float CosSunAngularRadius;
	float3 SunIrradiance;
	float SinSunAngularRadius;
	float3 CameraPosWS;
	float ShadowNormalBias;

	uint NumZTiles;
	uint NumXTiles;
	uint NumXYTiles;
	uint ClusterTileSize;

	float NearClip;
	float FarClip;
	float Padding0;
	float Padding1;

	float2 RTSize;
	float2 JitterOffset;

	SH9Color EnvSH;
};

struct LightConstants
{
	SpotLight Lights[MaxSpotLights];
	float4x4 ShadowMatrices[MaxSpotLights];
};

#endif