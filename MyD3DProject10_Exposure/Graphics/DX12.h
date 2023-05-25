#pragma once

#include "..\\PCH.h"

namespace Framework
{

namespace DX12
{
// Constants
const uint64 RenderLatency = 2;

// Externals
extern ID3D12Device5* Device;
extern ID3D12GraphicsCommandList4* CmdList;
extern ID3D12CommandQueue* GfxQueue;
extern D3D_FEATURE_LEVEL FeatureLeavel;
extern IDXGIFactory4* Factory;
extern IDXGIAdapter1* Adapter;

extern uint64 CurrentCPUFrame;	// Total number of CPU frames completed (completed means all command buffers submitted to the GPU)
extern uint64 CurrentGPUFrame;	// Total number of GPU frames completed (completed means that the GPU signals the fence)
extern uint64 CurrentFrameIdx;	// CurrentCPUFrame % RenderLatency

// Lifetime
void Initialize(D3D_FEATURE_LEVEL minFeatureLevel, uint32 adapterIdx);
void Shutdown();

// Frame submission synchronization
void BeginFrame();
void EndFrame(IDXGISwapChain4* swapChain, uint32 synInterval);
void FlushGPU();

void DeferredRelease_(IUnknown* resource);

template<typename T> void DeferredRelease(T*& resource)
{
	IUnknown* base = resource;
	DeferredRelease_(base);
	resource = nullptr;
}

template<typename T> void Release(T*& resource)
{
	if (resource != nullptr) {
		resource->Release();
		resource = nullptr;
	}
}

void DeferredCreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, uint32 descriptorIdx);

} // namespace DX12
} // namespace Framework