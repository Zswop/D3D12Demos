//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------

float3 Fresnel(in float3 specAlbedo, in float3 h, in float3 l)
{
	float3 fresnel = specAlbedo + (1.0f - specAlbedo) * pow((1.0f - saturate(dot(l, h))), 5.0f);

	// Fade out spec entirely when lower than 0.1% albedo
	fresnel *= saturate(dot(specAlbedo, 333.0f));

	return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Helper for computing the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXV1(in float m2, in float nDotX)
{
	return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}

//-------------------------------------------------------------------------------------------------
// Smith Joint GGX
// Note: Vis = G2 / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// Heitz http://jcgt.org/published/0003/02/03/paper.pdf
// https://twvideo01.ubm-us.net/o1/vault/gdc2017/Presentations/Hammon_Earl_PBR_Diffuse_Lighting.pdf
//-------------------------------------------------------------------------------------------------
float GGX_V(in float m2, in float nDotL, in float nDotV)
{
	float GGXV = nDotL * sqrt(nDotV * nDotV * (1.0 - m2) + m2);
	float GGXL = nDotV * sqrt(nDotL * nDotL * (1.0 - m2) + m2);
	return 0.5f / (GGXV + GGXL);
}

float GGXVisibility(in float m2, in float nDotL, in float nDotV)
{
	return GGXV1(m2, nDotL) * GGXV1(m2, nDotV);
}

float SmithGGXMasking(float3 n, float3 l, float3 v, float a2)
{
	float dotNL = saturate(dot(n, l));
	float dotNV = saturate(dot(n, v));
	float denomC = sqrt(a2 + (1.0f - a2) * dotNV * dotNV) + dotNV;

	return 2.0f * dotNV / denomC;
}

float SmithGGXMaskingShadowing(float3 n, float3 l, float3 v, float a2)
{
	float dotNL = saturate(dot(n, l));
	float dotNV = saturate(dot(n, v));

	float denomA = dotNV * sqrt(a2 + (1.0f - a2) * dotNL * dotNL);
	float denomB = dotNL * sqrt(a2 + (1.0f - a2) * dotNV * dotNV);

	return 2.0f * dotNL * dotNV / (denomA + denomB);
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a GGX microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float GGX_Specular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
	float nDotH = saturate(dot(n, h));
	float nDotL = saturate(dot(n, l));
	float nDotV = saturate(dot(n, v));

	float m2 = m * m;

	// Calcalate the distribution term
	float d = m2 / (Pi * pow(nDotH * nDotH * (m2 - 1) + 1, 2.0f));

	// Computes the GGX visibility term
	float vis = GGXVisibility(m2, nDotL, nDotV);

	return d * vis;
}

//-------------------------------------------------------------------------------------------------
// Returns scale and bias values for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float2 GGXEnvironmentBRDFScaleBias(in float nDotV, in float sqrtRoughness)
{
	const float nDotV2 = nDotV * nDotV;
	const float sqrtRoughness2 = sqrtRoughness * sqrtRoughness;
	const float sqrtRoughness3 = sqrtRoughness2 * sqrtRoughness;

	const float delta = 0.991086418474895f + (0.412367709802119f * sqrtRoughness * nDotV2) -
		(0.363848256078895f * sqrtRoughness2) -
		(0.758634385642633f * nDotV * sqrtRoughness2);
	const float bias = saturate((0.0306613448029984f * sqrtRoughness) + 0.0238299731830387f /
		(0.0272458171384516f + sqrtRoughness3 + nDotV2) -
		0.0454747751719356f);

	const float scale = saturate(delta - bias);
	return float2(scale, bias);
}

//-------------------------------------------------------------------------------------------------
// Calculates the lighting result for an analytical light source
//-------------------------------------------------------------------------------------------------
float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 peekIrradiance, 
	in float3 diffuseAlbedo, in float3 specularAlbedo, in float roughness, in float3 positionWS, 
	in float3 cameraPosWS, in float3 msEnergyCompensation)
{
	//float3 lighting = diffuseAlbedo;
	float3 lighting = diffuseAlbedo * InvPi;

	float3 view = normalize(cameraPosWS - positionWS);
	const float nDotL = saturate(dot(normal, lightDir));
	if (nDotL > 0.0f)
	{
		const float nDotV = saturate(dot(normal, view));
		float3 h = normalize(view + lightDir);

		float3 fresnel = Fresnel(specularAlbedo, h, lightDir);

		float specular = GGX_Specular(roughness, normal, h, view, lightDir);
		lighting += specular * fresnel * msEnergyCompensation;
	}

	return lighting * nDotL * peekIrradiance;
}
