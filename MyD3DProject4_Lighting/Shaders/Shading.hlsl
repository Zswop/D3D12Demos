#include "DescriptorTables.hlsl"
#include "Constants.hlsl"
#include "BRDF.hlsl"

struct MaterialTextureIndices
{
	uint Albedo;
	uint Normal;
	uint Roughness;
	uint Metallic;
};

struct ShadingConstants
{
	float3 SunDirectionWS;
	float CosSunAngularRadius;
	float3 SunIrradiance;
	float SinSunAngularRadius;
	float3 CameraPosWS;
};

//-------------------------------------------------------------------------------------------------
// Calculates the lighting result for an analytical light source
//-------------------------------------------------------------------------------------------------
float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 peekIrradiance, in float3 diffuseAlbedo,
	in float3 specularAlbedo, in float roughness, in float3 positionWS, in float3 cameraPosWS)
{
	float3 lighting = diffuseAlbedo;
	//float3 lighting = diffuseAlbedo * (1.0f / 3.14159f);

	float3 view = normalize(cameraPosWS - positionWS);
	const float nDotL = saturate(dot(normal, lightDir));
	if (nDotL > 0.0f)
	{
		const float nDotV = saturate(dot(normal, view));
		float3 h = normalize(view + lightDir);

		float3 fresnel = Fresnel(specularAlbedo, h, lightDir);

		float specular = GGX_Specular(roughness, normal, h, view, lightDir);
		lighting += specular * fresnel;
	}

	return lighting * nDotL * peekIrradiance;
}