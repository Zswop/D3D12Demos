#include "DescriptorTables.hlsl"

struct VSInput
{
	float2 Position : POSITION;
	float4 Color : COLOR;
	float2 UV : UV;
};

struct VSOutput
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
	float2 UV : UV;
};

struct ImGuiConstants
{
	row_major float4x4 ProjectionMatrix;
};

struct SRVIndicesLayout
{
	uint TextureIdx;
};

SamplerState LinearSampler : register(s0);
ConstantBuffer<ImGuiConstants> ImGuiCB : register(b0);
ConstantBuffer<SRVIndicesLayout> SRVIndices : register(b1);

VSOutput ImGuiVS(in VSInput input)
{
	VSOutput output;
	output.Position = mul(float4(input.Position, 0.0, 1.0f), ImGuiCB.ProjectionMatrix);
	output.Color = input.Color;
	output.UV = input.UV;

	return output;
}

float4 ImGuiPS(in VSOutput input) : SV_Target0
{
	Texture2D ImGuiTexture = Tex2DTable[SRVIndices.TextureIdx];
	return ImGuiTexture.Sample(LinearSampler, input.UV) * input.Color;
}