#ifndef SHAREDTYPES_HLSL_
#define SHAREDTYPES_HLSL_

static const uint MaxSpotLights = 32;
static const uint SpotLightElementsPerCluster = 1;
static const float SpotShadowNearClip = 0.1f;

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

struct LightConstants
{
	SpotLight Lights[MaxSpotLights];
	float4x4 ShadowMatrices[MaxSpotLights];
};

struct GeometryInfo
{
	uint VtxOffset;
	uint IdxOffset;
	uint MaterialIdx;
	uint PadTo16Bytes;
};

struct MeshVertex
{
	float3 Position;
	float3 Normal;
	float2 UV;
	float3 Tangent;
	float3 Bitangent;
	float4 Color;
};

#endif