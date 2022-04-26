#pragma once

#include "..\PCH.h"

#include "DX12.h"

namespace Framework
{

// Forward declarations
struct DescriptorHeap;
struct DescriptorHandle;

namespace DX12
{

const uint32 StandardMSAAPattern = 0xFFFFFFFF;

extern uint32 RTVDescriptorSize;
extern uint32 SRVDescriptorSize;
extern uint32 UAVDescriptorSize;
extern uint32 CBVDescriptorSize;
extern uint32 DSVDescriptorSize;

extern DescriptorHeap RTVDescriptorHeap;
extern DescriptorHeap SRVDescriptorHeap;
extern DescriptorHeap DSVDescriptorHeap;
extern DescriptorHeap UAVDescriptorHeap;

// Lifetime
void Initialize_Helpers();
void Shutdown_Helpers();

void EndFrame_Helpers();

// Resource Barriers
void TransitionResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after, uint32 subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

// Resource management
//uint64 GetResourceSize(const D3D12_RESOURCE_DESC& desc, uint32 firstSubResource = 0, uint32 numSubResources = 1);
//uint64 GetResourceSize(ID3D12Resource* resource, uint32 firstSubResource = 0, uint32 numSubResources = 1);

// Heap helpers
const D3D12_HEAP_PROPERTIES* GetDefaultHeapProps();
const D3D12_HEAP_PROPERTIES* GetUploadHeapProps();
const D3D12_HEAP_PROPERTIES* GetReadbackHeapProps();

// Convenience functions
void SetViewport(ID3D12GraphicsCommandList* cmdList, uint64 width, uint64 height, float zMin = 0.0f, float zMax = 1.0f);

// Resource binding
void SetDescriptorHeaps(ID3D12GraphicsCommandList* cmdList);

}
}