#include "DescriptorTables.hlsl"
#include "Constants.hlsl"
#include "BRDF.hlsl"
#include "SH.hlsl"

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

	SH9Color EnvSH;
};

struct ShadingInput
{
	uint2 PositionSS;
	float3 PositionWS;
	float DepthVS;
	float3x3 TangentFrame;

	float4 AlbedoMap;
	float2 NormalMap;
	float RoughnessMap;
	float MetallicMap;

	SamplerState AnisoSampler;

	ShadingConstants ShadingCBuffer;
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

float3 CalcSpecularIBL(in float3 normal, in float3 viewWS, in float3 specularAlbedo, in float roughness,
	in TextureCube specularCubemap, in Texture2D specularLUT, in SamplerState linearSampler)
{
	float2 cubMapSize;
	uint numMips;
	specularCubemap.GetDimensions(0, cubMapSize.x, cubMapSize.y, numMips);

	float3 reflectWS = reflect(-viewWS, normal);

	const float SqrtRoughness = sqrt(roughness);

	// Compute the mip level, assuming the top level is a roughness of 0.01
	float mipLevel = saturate(SqrtRoughness - 0.01f) * (numMips - 1.0f);
	
	float viewAngle = saturate(dot(viewWS, normal));

	float3 prefilteredColor = specularCubemap.SampleLevel(linearSampler, reflectWS, mipLevel).xyz;
	float2 envBRDF = specularLUT.SampleLevel(linearSampler, float2(viewAngle, SqrtRoughness), 0.0f).xy;
	
	return prefilteredColor * (specularAlbedo * envBRDF.x + envBRDF.y);
}

//-------------------------------------------------------------------------------------------------
// Calculates the full shading result for a single pixel. Note: some of the input textures
// are passed directly to this function instead of through the ShadingInput struct in order to
// work around incorrect behavior from the shader compiler
//-------------------------------------------------------------------------------------------------
float3 ShadePixel(in ShadingInput input, in TextureCube specularCubemap, in Texture2D specularLUT, in SamplerState linearSampler)
{
	float3 vtxNormalWS = input.TangentFrame._m20_m21_m22;
	float3 positionWS = input.PositionWS;

	const ShadingConstants CBuffer = input.ShadingCBuffer;

	float3 viewWS = normalize(CBuffer.CameraPosWS - positionWS);

	// Sample the normal map, and convert the normal to world space
	float3 normalTS;
	normalTS.xy = input.NormalMap.xy * 2.0f - 1.0f;
	normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
	float3 normalWS = normalize(mul(normalTS, input.TangentFrame));

	float roughness = input.RoughnessMap * input.RoughnessMap;
	float metallic = saturate(input.MetallicMap);
	//metallic = 0.8f;
	//roughness = 0.1f;

	float4 albedoMap = input.AlbedoMap;
	float3 diffuseAlbedo = lerp(albedoMap.xyz, 0.0f, metallic);
	float3 specularAlbedo = lerp(0.03f, albedoMap.xyz, metallic);

	float depthVS = input.DepthVS;
	float3 cameraPosWS = CBuffer.CameraPosWS;
	float3 sunDirectionWS = CBuffer.SunDirectionWS;
	float3 sunIrradiance = CBuffer.SunIrradiance;
	
	float3 output = 0.0f;

	// Add in the primary directional light
	output += CalcLighting(normalWS, sunDirectionWS, sunIrradiance, diffuseAlbedo, specularAlbedo,
		roughness, positionWS, cameraPosWS);

	// Add in the ambient
	{
		const float occlusion = 0.1f; // Darken the ambient since we don't have any sky occlusion
		float3 ambient = EvalSH9Irradiance(normalWS, CBuffer.EnvSH) * InvPi;
		output += ambient * diffuseAlbedo * occlusion;
		
		output += CalcSpecularIBL(normalWS, viewWS, specularAlbedo, roughness, specularCubemap, 
			specularLUT, linearSampler) * occlusion;
	}

	output = clamp(output, 0.0f, FP16Max);
	return output;
}