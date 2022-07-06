//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------

struct SpdConstants
{
	uint Mips;
	uint NumWorkGroups;
};

ConstantBuffer<SpdConstants> CBuffer : register(b0);

//=================================================================================================
// Resources
//=================================================================================================

struct SpdGlobalAtomicBuffer
{
	uint counter[6];
};

globallycoherent RWStructuredBuffer<SpdGlobalAtomicBuffer> spdGlobalAtomic : register(u0);

globallycoherent RWTexture2DArray<float4>            imgDst6         :  register(u1);

RWTexture2DArray<float4>                             imgDst[13]      :  register(u2); // don't access MIP [6]

#define A_GPU
#define A_HLSL

#include "ffx-spd\\ffx_a.h"

groupshared AU1 spdCounter;

groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];
groupshared AF1 spdIntermediateB[16][16];
groupshared AF1 spdIntermediateA[16][16];

AF4 SpdLoadSourceImage(AF2 tex, AU1 slice)
{
	return imgDst[0][float3(tex, slice)];
}

AF4 SpdLoad(ASU2 tex, AU1 slice)
{
	return imgDst6[uint3(tex, slice)];
}

void SpdStore(ASU2 pix, AF4 outValue, AU1 index, AU1 slice)
{
	if (index == 5)
	{
		imgDst6[uint3(pix, slice)] = outValue;
		return;
	}
	imgDst[index + 1][uint3(pix, slice)] = outValue;
}

void SpdIncreaseAtomicCounter(AU1 slice)
{
	InterlockedAdd(spdGlobalAtomic[0].counter[slice], 1, spdCounter);
}

AU1 SpdGetAtomicCounter()
{
	return spdCounter;
}

void SpdResetAtomicCounter(AU1 slice)
{
	spdGlobalAtomic[0].counter[slice] = 0;
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y)
{
	return AF4(
		spdIntermediateR[x][y],
		spdIntermediateG[x][y],
		spdIntermediateB[x][y],
		spdIntermediateA[x][y]);
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
	spdIntermediateR[x][y] = value.x;
	spdIntermediateG[x][y] = value.y;
	spdIntermediateB[x][y] = value.z;
	spdIntermediateA[x][y] = value.w;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
	return (v0 + v1 + v2 + v3) * 0.25;
}

#include "ffx-spd\\ffx_spd.h"

// Main function
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void GenerateMipsCS(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
	SpdDownsample(
		AU2(WorkGroupId.xy),
		AU1(LocalThreadIndex),
		AU1(CBuffer.Mips),
		AU1(CBuffer.NumWorkGroups),
		AU1(WorkGroupId.z),
		AU2(0, 0));
}