#include <DescriptorTables.hlsl>

//=================================================================================================
// Constant buffers
//=================================================================================================

struct PSConstants
{
	row_major float4x4 InvViewProj;
	row_major float4x4 PreViewProjection;

	float2 JitterOffset;
	float2 RTSize;
};

struct SRVIndexConstants
{
	uint DepthMapIdx;
};

ConstantBuffer<PSConstants> PSCBuffer : register(b0);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b1);

//=================================================================================================
// Input/Output structs
//=================================================================================================

struct PSInput
{
	float4 PositionSS			: SV_Position;
	float2 TexCoord				: TEXCOORD;
};

//=================================================================================================
// VelocityBuffer Pixel Shader
//=================================================================================================

// Computes world-space position from post-projection depth
float3 PositionFromDepth(in float zw, in float2 uv)
{
	float4 positionCS = float4(uv * 2.0f - 1.0f, zw, 1.0f);
	positionCS.y *= -1.0f;
	float4 positionWS = mul(positionCS, PSCBuffer.InvViewProj);
	return positionWS.xyz / positionWS.w;
}

float2 VelocityPS(in PSInput input) : SV_Target
{
	Texture2D DepthMap = Tex2DTable[SRVIndices.DepthMapIdx];
	uint2 pixelPos = uint2(input.PositionSS.xy);

	float zw = DepthMap[pixelPos].x;
	float3 positionWS = PositionFromDepth(zw, input.TexCoord);

	float4 prevPositionCS = mul(float4(positionWS, 1.0), PSCBuffer.PreViewProjection);
	float2 prevPositionSS = (prevPositionCS.xy / prevPositionCS.w) * float2(0.5f, -0.5f) + 0.5f;
	prevPositionSS *= PSCBuffer.RTSize;

	float2 velocity = input.PositionSS.xy - prevPositionSS;
	velocity -= PSCBuffer.JitterOffset;
	velocity /= PSCBuffer.RTSize;
	return velocity;
}