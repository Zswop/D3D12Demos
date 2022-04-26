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
	uint SunShadowMapIdx;
	uint MaterialTextureIndicesIdx;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<ShadingConstants> PSCBuffer : register(b0);
ConstantBuffer<SunShadowConstants> ShadowCBuffer : register(b1);
ConstantBuffer<MatIndexConstants> MatIndexCBuffer : register(b2);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b3);

//=================================================================================================
// Resources
//=================================================================================================

StructuredBuffer<MaterialTextureIndices> MaterialIndicesBuffers[] : register(t0, space100);

SamplerState AnisoSampler : register(s0);
SamplerComparisonState ShadowMapSampler : register(s1);

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

	ShadingInput shadingInput;
	shadingInput.PositionWS = input.PositionWS;
	shadingInput.DepthVS = input.DepthVS;
	shadingInput.TangentFrame = tangentFrame;

	shadingInput.AlbedoMap = AlbedoMap.Sample(AnisoSampler, input.UV);
	shadingInput.NormalMap = NormalMap.Sample(AnisoSampler, input.UV).xy;
	shadingInput.RoughnessMap = RoughnessMap.Sample(AnisoSampler, input.UV).x;
	shadingInput.MetallicMap = MetallicMap.Sample(AnisoSampler, input.UV).x;

	shadingInput.ShadingCBuffer = PSCBuffer;
	shadingInput.ShadowCBuffer = ShadowCBuffer;

	Texture2DArray sunShadowMap = Tex2DArrayTable[SRVIndices.SunShadowMapIdx];
	float3 shadingResult = ShadePixel(shadingInput, sunShadowMap, ShadowMapSampler);

	//float nDotL = dot(sunDirectionWS, normalWS);
	//return float4(nDotL, nDotL, nDotL, nDotL);

	return float4(shadingResult, 1.0f);
}