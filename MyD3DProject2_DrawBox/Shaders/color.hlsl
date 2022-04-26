cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj; 
};

struct VertexIn
{
	float3 PositionOS 		    : POSITION;
    float3 NormalOS 		    : NORMAL;
    float2 UV 		            : UV;
	float3 TangentOS 		    : TANGENT;
	float3 BitangentOS		    : BITANGENT;
    float4 Color 				: COLOR;
};

struct VertexOut
{
	float4 PositionCS  	: SV_POSITION;
    float4 Color 		: COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// Transform to homogeneous clip space.
	vout.PositionCS = float4(vin.PositionOS, 1.0f); //mul(float4(vin.PositionOS, 1.0f), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}


