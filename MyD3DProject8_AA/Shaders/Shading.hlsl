#include "DescriptorTables.hlsl"
#include "Constants.hlsl"
#include "Shadows.hlsl"
#include "BRDF.hlsl"
#include "SH.hlsl"

#include "SharedTypes.hlsl"

static const uint SpotLightElementsPerCluster = 1;

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
	float3 EmissiveMap;

	ByteAddressBuffer SpotLightClusterBuffer;

	ShadingConstants ShadingCBuffer;
	SunShadowConstants ShadowCBuffer;
	LightConstants LightCBuffer;
};

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
float3 ShadePixel(in ShadingInput input, in TextureCube specularCubemap, in Texture2D specularLUT, in SamplerState linearSampler,
	in Texture2DArray sunShadowMap, in Texture2DArray spotLightShadowMap, in SamplerComparisonState shadowSampler)
{
	float3 vtxNormalWS = input.TangentFrame._m20_m21_m22;
	float3 positionWS = input.PositionWS;

	const ShadingConstants CBuffer = input.ShadingCBuffer;
	const SunShadowConstants ShadowCBuffer = input.ShadowCBuffer;

	float3 viewWS = normalize(CBuffer.CameraPosWS - positionWS);

	// Sample the normal map, and convert the normal to world space
	float3 normalTS;
	normalTS.xy = input.NormalMap.xy * 2.0f - 1.0f;
	normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
	float3 normalWS = normalize(mul(normalTS, input.TangentFrame));
	
	float4 albedoMap = input.AlbedoMap;
	float metallic = saturate(input.MetallicMap);
	float roughness = input.RoughnessMap * input.RoughnessMap;

	float3 diffuseAlbedo = lerp(albedoMap.xyz, 0.0f, metallic);
	float3 specularAlbedo = lerp(0.03f, albedoMap.xyz, metallic);

	float depthVS = input.DepthVS;
	float3 cameraPosWS = CBuffer.CameraPosWS;
	float3 sunDirectionWS = CBuffer.SunDirectionWS;
	float3 sunIrradiance = CBuffer.SunIrradiance;
	
	float3 output = 0.0f;

	// Add in the primary directional light
	{
		float2 shadowMapSize;
		float numSlices;
		sunShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);

		const float3 shadowPosOffset = GetShadowPosOffset(saturate(dot(vtxNormalWS, sunDirectionWS)), vtxNormalWS, shadowMapSize.x);

		float sunShadowVisibility = SunShadowVisibility(positionWS, depthVS, vtxNormalWS, shadowPosOffset, 0.0f,
			sunShadowMap, shadowSampler, ShadowCBuffer);
		// sunShadowVisibility = 1.0f;
		// return float3(sunShadowVisibility, 0, 0);

		output += CalcLighting(normalWS, sunDirectionWS, sunIrradiance, diffuseAlbedo, specularAlbedo,
			roughness, positionWS, cameraPosWS) * sunShadowVisibility;
	}
	
	// Add in the other lights
	uint numLights = 0;
	{
		uint2 pixelPos = input.PositionSS;
		float zRange = CBuffer.FarClip - CBuffer.NearClip;
		float normalizedZ = saturate((depthVS - CBuffer.NearClip) / zRange);
		uint zTile = normalizedZ * CBuffer.NumZTiles;
		
		uint3 tileCoords = uint3(pixelPos / CBuffer.ClusterTileSize, zTile);
		uint clusterIdx = (tileCoords.z * CBuffer.NumXYTiles) + (tileCoords.y * CBuffer.NumXTiles) + tileCoords.x;
		uint clusterOffset = clusterIdx * SpotLightElementsPerCluster;

		float2 shadowMapSize;
		float numSlices;
		spotLightShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);

		[unroll]
		for (uint elemIdx = 0; elemIdx < SpotLightElementsPerCluster; ++elemIdx)
		{
			// Loop until we've processed every raised bit
			uint clusterElemMask = input.SpotLightClusterBuffer.Load((clusterOffset + elemIdx) * 4);

			while (clusterElemMask)
			{
				uint bitIdx = firstbitlow(clusterElemMask);
				clusterElemMask &= ~(1 << bitIdx);
				uint spotLightIdx = bitIdx + (elemIdx * 32);
				SpotLight spotLight = input.LightCBuffer.Lights[spotLightIdx];

				float3 surfaceToLight = spotLight.Position - positionWS;
				float distanceToLight = length(surfaceToLight);
				surfaceToLight /= distanceToLight;
				float angleFactor = saturate(dot(surfaceToLight, spotLight.Direction));
				float angularAttenuation = smoothstep(spotLight.AngularAttenuationY, spotLight.AngularAttenuationX, angleFactor);
				
				if (angularAttenuation > 0.0f)
				{
					float d = distanceToLight / spotLight.Range;
					float falloff = saturate(1.0f - d * d * d * d);
					falloff = (falloff * falloff) / (distanceToLight * distanceToLight + 1.0f);
					float3 intensity = spotLight.Intensity * angularAttenuation * falloff;

					const float3 shadowPosOffset = GetShadowPosOffset(saturate(dot(vtxNormalWS, surfaceToLight)), vtxNormalWS, shadowMapSize.x);

					const float SpotShadowNearClip = 0.1f;
					float spotLightVisibility = SpotLightShadowVisibility(positionWS, input.LightCBuffer.ShadowMatrices[spotLightIdx],
						spotLightIdx,shadowPosOffset, spotLightShadowMap, shadowSampler, float2(SpotShadowNearClip, spotLight.Range));
					spotLightVisibility = 1.0f;

					output += CalcLighting(normalWS, surfaceToLight, intensity, diffuseAlbedo, specularAlbedo,
						roughness, positionWS, cameraPosWS) * spotLightVisibility;
				}

				++numLights;
			}
		}

		//output = lerp(output, float3(2.5f, 0.0f, 0.0f), numLights / 10.0f);
	}

	// Add in the ambient
	{
		const float occlusion = 0.1f; // Darken the ambient since we don't have any sky occlusion
		float3 ambient = EvalSH9Irradiance(normalWS, CBuffer.EnvSH) * InvPi;
		output += ambient * diffuseAlbedo * occlusion;

		output += CalcSpecularIBL(normalWS, viewWS, specularAlbedo, roughness, specularCubemap,
			specularLUT, linearSampler) * occlusion;
	}

	// Add in the emission
	{
		output += input.EmissiveMap;
	}

	output = clamp(output, 0.0f, FP16Max);
	return output;
}