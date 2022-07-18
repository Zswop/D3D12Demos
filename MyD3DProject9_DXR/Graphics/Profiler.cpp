#include "Profiler.h"

namespace Framework
{

Profiler Profiler::GlobalProfiler;

static const uint64 MaxProfiles = 64;

struct ProfileData
{
	const char* Name = nullptr;

	bool QueryStarted = false;
	bool QueryFinished = false;
	bool Active = false;

	bool CPUProfile = false;
	int64 StartTime = 0;
	int64 EndTime = 0;

	static const uint64 FilterSize = 64;
	double TimeSamples[FilterSize] = { };
	uint64 CurrSample = 0;
};

void Profiler::Initialize()
{
	Shutdown();

	enableGPUProfiling = true;

	D3D12_QUERY_HEAP_DESC heapDesc = { };
	heapDesc.Count = MaxProfiles * 2;
	heapDesc.NodeMask = 0;
	heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	DX12::Device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap));

	readbackBuffer.Initialize(MaxProfiles * DX12::RenderLatency * 2 * sizeof(uint64));
	readbackBuffer.Resource->SetName(L"Query Readback Buffer");

	profiles.Init(MaxProfiles);
	cpuProfiles.Init(MaxProfiles);
}

void Profiler::Shutdown()
{
	DX12::DeferredRelease(queryHeap);
	readbackBuffer.Shutdown();
	cpuProfiles.Shutdown();
	profiles.Shutdown();
	numProfiles = 0;
}

uint64 Profiler::StartProfile(ID3D12GraphicsCommandList* cmdList, const char* name)
{
	Assert_(name != nullptr);
	if (enableGPUProfiling == false)
		return uint64(-1);

	uint64 profileIdx = uint64(-1);
	for (uint64 i = 0; i < numProfiles; ++i)
	{
		if (profiles[i].Name == name) 
		{
			profileIdx = i;
			break;
		}
	}

	if (profileIdx == uint64(-1))
	{
		Assert_(numProfiles < MaxProfiles);
		profileIdx = numProfiles++;
		profiles[profileIdx].Name = name;
	}

	ProfileData& profileData = profiles[profileIdx];
	Assert_(profileData.QueryStarted == false);
	Assert_(profileData.QueryFinished = false);
	profileData.CPUProfile = false;
	profileData.Active = true;

	// Insert the start timestamp
	const uint32 startQueryIdx = uint32(profileIdx * 2);
	cmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);

	profileData.QueryStarted = true;
	return profileIdx;
}

void Profiler::EndProfile(ID3D12GraphicsCommandList* cmdList, uint64 idx)
{
	if (enableGPUProfiling == false)
		return;

	Assert_(idx < numProfiles);

	ProfileData& profileData = profiles[idx];
	Assert_(profileData.QueryStarted == true);
	Assert_(profileData.QueryFinished == false);

	// Insert the end timestamp
	const uint32 startQueryIdx = uint32(idx * 2);
	const uint32 endQueryIdx = startQueryIdx + 1;
	cmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);

	// Resolve the data
	const uint64 dstOffset = ((DX12::CurrentFrameIdx * MaxProfiles * 2) + startQueryIdx) * sizeof(uint64);
	cmdList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx, 2, readbackBuffer.Resource, dstOffset);

	profileData.QueryStarted = false;
	profileData.QueryFinished = true;
}


}