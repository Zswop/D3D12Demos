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
	row_major float4x4 PrevWorldViewProjection;
};

struct MatIndexConstants
{
	uint MatIndex;
};

struct SRVIndexConstants
{
	uint SunShadowMapIdx;
	uint SpotLightShadowMapIdx;
	uint MaterialTextureIndicesIdx;
	uint SpecularCubemapLookupIdx;
	uint SpecularCubemapIdx;
	uint SpotLightClusterBufferIdx;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<ShadingConstants> PSCBuffer : register(b0);
ConstantBuffer<SunShadowConstants> ShadowCBuffer : register(b1);
ConstantBuffer<MatIndexConstants> MatIndexCBuffer : register(b2);
ConstantBuffer<LightConstants> LightCBuffer : register(b3);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b4);

//=================================================================================================
// Resources
//=================================================================================================

StructuredBuffer<MaterialTextureIndices> MaterialIndicesBuffers[] : register(t0, space100);

SamplerState AnisoSampler : register(s0);
SamplerState LinearSampler : register(s1);
SamplerComparisonState ShadowMapSampler : register(s2);

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
	float DepthVS : DEPTHVS;

	float3 PositionWS           : POSITIONWS;	
	float3 PrevPosition			: PREVPOSITION;

	float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
	float2 UV 		            : UV;
};

struct PSInput
{
	float4 PositionSS 		    : SV_Position;

	float3 NormalWS 		    : NORMALWS;
	float DepthVS				: DEPTHVS;

	float3 PositionWS           : POSITIONWS;
	float3 PrevPosition			: PREVPOSITION;
	
	float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
	float2 UV 		            : UV;
};

struct PSOutputForward
{
	float4 Color : SV_Target0;
	float2 Velocity : SV_Target1;
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

	output.PrevPosition = mul(float4(input.PositionOS, 1.0f), VSCBuffer.PrevWorldViewProjection).xyw;

	output.TangentWS = normalize(mul(float4(input.TangentOS, 0.0f), VSCBuffer.World)).xyz;
	output.BitangentWS = normalize(mul(float4(input.BitangentOS, 0.0f), VSCBuffer.World)).xyz;

	output.UV = input.UV;

	return output;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================

PSOutputForward PSForward(in PSInput input)
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
	Texture2D EmissiveMap = Tex2DTable[matIndices.Emissive];	

	ShadingInput shadingInput;
	shadingInput.PositionSS = uint2(input.PositionSS.xy);
	shadingInput.PositionWS = input.PositionWS;
	shadingInput.DepthVS = input.DepthVS;
	shadingInput.TangentFrame = tangentFrame;

	shadingInput.AlbedoMap = AlbedoMap.Sample(AnisoSampler, input.UV);
	shadingInput.NormalMap = NormalMap.Sample(AnisoSampler, input.UV).xy;
	shadingInput.RoughnessMap = RoughnessMap.Sample(AnisoSampler, input.UV).x;
	shadingInput.MetallicMap = MetallicMap.Sample(AnisoSampler, input.UV).x;
	shadingInput.EmissiveMap = EmissiveMap.Sample(AnisoSampler, input.UV).xyz;

	shadingInput.SpotLightClusterBuffer = RawBufferTable[SRVIndices.SpotLightClusterBufferIdx];

	shadingInput.AnisoSampler = AnisoSampler;

	shadingInput.ShadingCBuffer = PSCBuffer;
	shadingInput.ShadowCBuffer = ShadowCBuffer;
	shadingInput.LightCBuffer = LightCBuffer;

	Texture2D SpecularLUT = Tex2DTable[SRVIndices.SpecularCubemapLookupIdx];
	TextureCube SpecularCubemap = TexCubeTable[SRVIndices.SpecularCubemapIdx];

	Texture2DArray sunShadowMap = Tex2DArrayTable[SRVIndices.SunShadowMapIdx];
	Texture2DArray spotLightShadowMap = Tex2DArrayTable[SRVIndices.SpotLightShadowMapIdx];

#if AlphaTest_
	Texture2D OpacityMap = Tex2DTable[matIndices.Opacity];
	if (OpacityMap.Sample(AnisoSampler, input.UV).x < 0.35f)
		discard;
#endif

	float3 shadingResult = ShadePixel(shadingInput, SpecularCubemap, SpecularLUT, LinearSampler,
		sunShadowMap, spotLightShadowMap, ShadowMapSampler);

	//float nDotL = dot(sunDirectionWS, normalWS);
	//return float4(nDotL, nDotL, nDotL, nDotL);

	float2 prevPositionSS = (input.PrevPosition.xy / input.PrevPosition.z) * float2(0.5f, -0.5f) + 0.5f;
	float2 velocity = input.PositionSS.xy - prevPositionSS;
	velocity -= PSCBuffer.JitterOffset;
	velocity /= PSCBuffer.RTSize;

	PSOutputForward output;
	output.Color = float4(shadingResult, 1.0f);
	output.Velocity = velocity;
	return output;
}