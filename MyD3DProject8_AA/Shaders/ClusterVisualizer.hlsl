//=================================================================================================
// Includes
//=================================================================================================
#include "Constants.hlsl"
#include "DescriptorTables.hlsl"

static const uint SpotLightElementsPerCluster = 1;

//=================================================================================================
// Resources
//=================================================================================================
struct ClusterVisConstants
{
	row_major float4x4 Projection;

	float3 ViewMin;
	float NearClip;

	float3 ViewMax;
	float InvClipRange;

	uint NumZTiles;
	uint NumXTiles;	
	uint NumXYTiles;
	uint ClusterTileSize;
		
	float2 DisplaySize;
	uint DecalClusterBufferIdx;
	uint SpotLightClusterBufferIdx;
};

ConstantBuffer<ClusterVisConstants> CBuffer : register(b0);

// ================================================================================================
// Pixel shader for visualizing decal/light counts from an overhead view of the frustum
// ================================================================================================
float4 ClusterVisualizerPS(in float4 PositionSS : SV_Position, in float2 TexCoord : TEXCOORD) : SV_Target0
{
	ByteAddressBuffer spotLightClusterBuffer = RawBufferTable[CBuffer.SpotLightClusterBufferIdx];

	float3 viewPos = lerp(CBuffer.ViewMin, CBuffer.ViewMax, float3(TexCoord.x, 0.5f, 1.0f - TexCoord.y));
	float4 projectedPos = mul(float4(viewPos, 1.0f), CBuffer.Projection);
	projectedPos.xyz /= projectedPos.w;
	projectedPos.y *= -1.0f;
	projectedPos.xy = projectedPos.xy * 0.5f + 0.5f;

	float2 screenPos = projectedPos.xy * CBuffer.DisplaySize;
	float normalizedZPos = saturate((viewPos.z - CBuffer.NearClip) * CBuffer.InvClipRange);
	uint3 tileCoords = uint3(uint2(screenPos) / CBuffer.ClusterTileSize, normalizedZPos * CBuffer.NumZTiles);
	uint clusterIdx = (tileCoords.z * CBuffer.NumXYTiles) + (tileCoords.y * CBuffer.NumXTiles) + tileCoords.x;

	if (projectedPos.x < 0.0f || projectedPos.x > 1.0f || projectedPos.y < 0.0f || projectedPos.y > 1.0f)
		return 0.0f;

	float3 output = 0.05f;
	
	{
		uint numLights = 0;
		uint clusterOffset = clusterIdx * SpotLightElementsPerCluster;

		[unroll]
		for (uint elemIdx = 0; elemIdx < SpotLightElementsPerCluster; ++elemIdx)
		{
			uint clusterElemMask = spotLightClusterBuffer.Load((clusterOffset + elemIdx) * 4);
			numLights += countbits(clusterElemMask);
		}

		output.x += numLights / 10.0f;
	}

	return float4(output, 0.9f);
}