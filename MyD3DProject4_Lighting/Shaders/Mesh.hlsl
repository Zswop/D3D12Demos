//=================================================================================================
// Includes
//=================================================================================================

#include "Shading.hlsl"

//=================================================================================================
// Constant buffers
//=================================================================================================

struct VSConstants 
{
	row_major float4x4 World;
	row_major float4x4 View;
	row_major float4x4 WorldViewProjection;
};

struct MatIndexConstants
{
	uint MatIndex;
};

struct SRVIndexConstants
{
	uint MaterialTextureIndicesIdx;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<ShadingConstants> PSCBuffer : register(b0);
ConstantBuffer<MatIndexConstants> MatIndexCBuffer : register(b1);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b2);

//=================================================================================================
// Resources
//=================================================================================================

StructuredBuffer<MaterialTextureIndices> MaterialIndicesBuffers[] : register(t0, space100);

SamplerState AnisoSampler : register(s0);

//=================================================================================================
// Input/Output structs
//=================================================================================================

struct VSInput
{
	float3 PositionOS			: POSITION;
	float3 NormalOS				: NORMAL;
	float2 UV					: UV;
	float3 TangentOS			: TANGENT;
	float3 BitangentOS			: BITANGENT;
};

struct VSOutput
{
	float4 PositionCS 		    : SV_Position;

	float3 NormalWS 		    : NORMALWS;
	float3 PositionWS           : POSITIONWS;
	float DepthVS				: DEPTHVS;
	float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
	float2 UV 		            : UV;
};

struct PSInput
{
	float4 PositionSS 		    : SV_Position;

	float3 NormalWS 		    : NORMALWS;
	float3 PositionWS           : POSITIONWS;
	float DepthVS				: DEPTHVS;
	float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
	float2 UV 		            : UV;
};

//=================================================================================================
// Vertex Shader
//=================================================================================================

VSOutput VS(in VSInput input, in uint VertexID : SV_VertexID)
{
	VSOutput output;

	float3 positionOS = input.PositionOS;

	output.PositionWS = mul(float4(positionOS, 1.0f), VSCBuffer.World).xyz;

	output.PositionCS = mul(float4(positionOS, 1.0f), VSCBuffer.WorldViewProjection);
	output.DepthVS = output.PositionCS.w;

	// Assumes uniform scaling; otherwise, need to use inverse-transpose of world matrix.
	output.NormalWS = normalize(mul(float4(input.NormalOS, 1.0f), VSCBuffer.World)).xyz;

	output.TangentWS = normalize(mul(float4(input.TangentOS, 0.0f), VSCBuffer.World)).xyz;
	output.BitangentWS = normalize(mul(float4(input.BitangentOS, 0.0f), VSCBuffer.World)).xyz;

	output.UV = input.UV;

	return output;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================

float4 PSForward(in PSInput input) : SV_Target
{
	float3 vtxNormalWS = normalize(input.NormalWS);
	float3 positionWS = input.PositionWS;

	float3 tangentWS = normalize(input.TangentWS);
	float3 bitangentWS = normalize(input.BitangentWS);
	float3x3 tangentFrame = float3x3(tangentWS, bitangentWS, vtxNormalWS);

	StructuredBuffer<MaterialTextureIndices> matIndicesBuffer = MaterialIndicesBuffers[SRVIndices.MaterialTextureIndicesIdx];
	MaterialTextureIndices matIndices = matIndicesBuffer[MatIndexCBuffer.MatIndex];
	Texture2D AlbedoMap = Tex2DTable[matIndices.Albedo];
	Texture2D NormalMap = Tex2DTable[matIndices.Normal];
	Texture2D RoughnessMap = Tex2DTable[matIndices.Roughness];
	Texture2D MetallicMap = Tex2DTable[matIndices.Metallic];

	float4 albedo = AlbedoMap.Sample(AnisoSampler, input.UV);
	float2 normalMap = NormalMap.Sample(AnisoSampler, input.UV).xy;
	float roughness = RoughnessMap.Sample(AnisoSampler, input.UV).x;
	float metallic = MetallicMap.Sample(AnisoSampler, input.UV).x;

	float3 sunDirectionWS = normalize(PSCBuffer.SunDirectionWS);
	float3 sunIrradiance = PSCBuffer.SunIrradiance;
	float3 cameraPosWS = PSCBuffer.CameraPosWS;

	float3 normalTS;
	normalTS.xy = normalMap * 2.0f - 1.0f;
	normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
	float3 normalWS = normalize(mul(normalTS, tangentFrame));

	float3 diffuseAlbedo = lerp(albedo.xyz, 0.0f, metallic);
	float3 specularAlbedo = lerp(0.04f, albedo.xyz, metallic);

	float3 shadingResult = CalcLighting(normalWS, sunDirectionWS, sunIrradiance, diffuseAlbedo,
		specularAlbedo, roughness, positionWS, cameraPosWS);

	//float nDotL = dot(sunDirectionWS, normalWS);
	//return float4(nDotL, nDotL, nDotL, nDotL);
	
	return float4(shadingResult, 1.0f);
}