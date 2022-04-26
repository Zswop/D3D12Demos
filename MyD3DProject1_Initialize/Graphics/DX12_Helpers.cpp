#include "DX12_Helpers.h"
#include "DX12.h"
#include "GraphicsTypes.h"

namespace Framework
{

namespace DX12
{

uint32 RTVDescriptorSize = 0;
uint32 SRVDescriptorSize = 0;
uint32 UAVDescriptorSize = 0;
uint32 CBVDescriptorSize = 0;
uint32 DSVDescriptorSize = 0;

DescriptorHeap RTVDescriptorHeap;
DescriptorHeap SRVDescriptorHeap;
DescriptorHeap DSVDescriptorHeap;
DescriptorHeap UAVDescriptorHeap;

void Initialize_Helpers()
{
	RTVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	SRVDescriptorHeap.Init(1024, 1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	DSVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
	UAVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);

	RTVDescriptorSize = RTVDescriptorHeap.DescriptorSize;
	SRVDescriptorSize = UAVDescriptorSize = CBVDescriptorSize = SRVDescriptorHeap.DescriptorSize;
	DSVDescriptorSize = DSVDescriptorHeap.DescriptorSize;
}

void Shutdown_Helpers()
{
	RTVDescriptorHeap.Shutdown();
	SRVDescriptorHeap.Shutdown();
	DSVDescriptorHeap.Shutdown();
	UAVDescriptorHeap.Shutdown();
}

void EndFrame_Helpers()
{
	RTVDescriptorHeap.EndFrame();
	SRVDescriptorHeap.EndFrame();
	DSVDescriptorHeap.EndFrame();
	UAVDescriptorHeap.EndFrame();
}

void TransitionResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 subResource)
{
	D3D12_RESOURCE_BARRIER barrier = { };
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = subResource;
	cmdList->ResourceBarrier(1, &barrier);
}

const D3D12_HEAP_PROPERTIES* GetDefaultHeapProps()
{
	static D3D12_HEAP_PROPERTIES heapProps =
	{
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0,
	};
	return &heapProps;
}

const D3D12_HEAP_PROPERTIES* GetUploadHeapProps()
{
	static D3D12_HEAP_PROPERTIES heapProps =
	{
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0,
	};

	return &heapProps;
}

const D3D12_HEAP_PROPERTIES* GetReadbackHeapProps()
{
	static D3D12_HEAP_PROPERTIES heapProps =
	{
		D3D12_HEAP_TYPE_READBACK,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0,
	};

	return &heapProps;
}

void SetViewport(ID3D12GraphicsCommandList* cmdList, uint64 width, uint64 height, float zMin, float zMax)
{
	D3D12_VIEWPORT viewport = { };
	viewport.Width = float(width);
	viewport.Height = float(height);
	viewport.MinDepth = zMin;
	viewport.MaxDepth = zMax;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	D3D12_RECT scissorRect = { };
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = uint32(width);
	scissorRect.bottom = uint32(height);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}

void SetDescriptorHeaps(ID3D12GraphicsCommandList* cmdList)
{
	ID3D12DescriptorHeap* heaps[] =
	{
		SRVDescriptorHeap.CurrentHeap(),
	};

	cmdList->SetDescriptorHeaps(ArraySize_(heaps), heaps);
}

}
}