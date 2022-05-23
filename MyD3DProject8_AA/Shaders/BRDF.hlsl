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
	float vis = GGX_V(m2, nDotL, nDotV);

	return d * vis;
}

//-------------------------------------------------------------------------------------------------
// Calculates the lighting result for an analytical light source
//-------------------------------------------------------------------------------------------------
float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 peekIrradiance, in float3 diffuseAlbedo,
	in float3 specularAlbedo, in float roughness, in float3 positionWS, in float3 cameraPosWS)
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
		lighting += specular * fresnel;
	}

	return lighting * nDotL * peekIrradiance;
}
