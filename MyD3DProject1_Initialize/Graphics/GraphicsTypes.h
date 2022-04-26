#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "..\\Utility.h"
#include "..\\Containers.h"
#include "DX12.h"
#include "DX12_Helpers.h"

namespace Framework
{

struct PersistentDescriptorAlloc
{
	D3D12_CPU_DESCRIPTOR_HANDLE Handles[DX12::RenderLatency] = { };
	uint32 Index = uint32(-1);
};

struct TempDescriptorAlloc
{
	D3D12_CPU_DESCRIPTOR_HANDLE StartCPUHandle = { };
	D3D12_GPU_DESCRIPTOR_HANDLE StartGPUHandle = { };
	uint32 StartIndex = uint32(-1);
};

struct DescriptorHeap
{
	ID3D12DescriptorHeap* Heaps[DX12::RenderLatency] = { };
	uint32 NumPersistent = 0;
	uint32 PersistentAllocated = 0;
	Array<uint32> DeadList;
	uint32 NumTemporary = 0;
	volatile int64 TemporaryAllocated = 0;
	uint32 HeapIndex = 0;
	uint32 NumHeaps = 0;
	uint32 DescriptorSize = 0;
	bool32 ShaderVisible = false;
	D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUStart[DX12::RenderLatency] = { };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUStart[DX12::RenderLatency] = { };
	SRWLOCK Lock = SRWLOCK_INIT;

	~DescriptorHeap();

	void Init(uint32 numPersistent, uint32 numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible);
	void Shutdown();

	PersistentDescriptorAlloc AllocatePersistent();
	void FreePersistent(uint32& idx);
	void FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle);
	void FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle);

	TempDescriptorAlloc AllocateTemporary(uint32 count);
	void EndFrame();

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32 descriptorIdx) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32 descriptorIdx) const;

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const;

	uint32 IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
	uint32 IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);

	ID3D12DescriptorHeap* CurrentHeap() const;
	uint32 TotalNumDescriptors() const { return NumPersistent + NumTemporary; }
};

struct Fence
{
	ID3D12Fence* D3DFence = nullptr;
	HANDLE FenceEvent = INVALID_HANDLE_VALUE;

	~Fence();

	void Init(uint64 initialValue = 0);
	void Shutdown();

	void Signal(ID3D12CommandQueue* queue, uint64 fenceValue);
	void Wait(uint64 fenceValue);
	bool Signaled(uint64 fenceValue);
	void Clear(uint64 fenceValue);
};

struct Texture
{
	uint32 SRV = uint32(-1);
	ID3D12Resource* Resource = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 Depth = 0;
	uint32 NumMips = 0;
	uint32 ArraySize = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	bool32 Cubemap = false;

	Texture();
	~Texture();

	bool Valid() const
	{
		return Resource != nullptr;
	}

	void Shutdown();
};

struct RenderTextureInit
{
	uint64 Width = 0;
	uint64 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	uint64 MSAASamples = 1;
	uint64 ArraySize = 1;
	bool32 CreateUAV = false;
	D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	const wchar* Name = nullptr;
};

struct RenderTexture
{
	Texture Texture;
	D3D12_CPU_DESCRIPTOR_HANDLE RTV = { };
	D3D12_CPU_DESCRIPTOR_HANDLE UAV = { };
	Array<D3D12_CPU_DESCRIPTOR_HANDLE> ArrayRTVs;
	uint32 MSAASamples = 1;
	uint32 MSAAQuality = 0;

	RenderTexture();
	~RenderTexture();

	void Initialize(const RenderTextureInit& init);
	void Shutdown();

	void Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, 
		uint64 mipLevel = uint64(-1), uint64 arraySlice = uint64(-1)) const;
	void MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel = uint64(-1), uint64 arraySlice = uint64(-1)) const;
	void MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel = uint64(-1), uint64 arraySlice = uint64(-1)) const;
	void UAVBarrier(ID3D12GraphicsCommandList* cmdList) const;

	uint32 SRV() const { return Texture.SRV; }
	uint64 Width() const { return Texture.Width; }
	uint64 Height() const { return Texture.Height; }
	DXGI_FORMAT Format() const { return Texture.Format; }
	ID3D12Resource* Resource() const { return Texture.Resource; }
	uint64 SubResourceIndex(uint64 mipLevel, uint64 arraySlice) const { return arraySlice * Texture.NumMips + mipLevel; }
};

struct DepthBufferInit
{
	uint64 Width = 0;
	uint64 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	uint64 MSAASamples = 1;
	uint64 ArraySize = 1;
	D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	const wchar* Name = nullptr;
};

struct DepthBuffer
{
	Texture Texture;
	D3D12_CPU_DESCRIPTOR_HANDLE DSV = { };
	D3D12_CPU_DESCRIPTOR_HANDLE ReadOnlyDSV = { };
	Array<D3D12_CPU_DESCRIPTOR_HANDLE> ArrayDSVs = { };
	uint32 MSAASamples = 1;
	uint32 MSAAQuality = 0;
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;

	DepthBuffer();
	~DepthBuffer();

	void Initialize(const DepthBufferInit& init);
	void Shutdown();

	void Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint64 arraySlice = uint64(-1)) const;
	void MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice = uint64(-1)) const;
	void MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice = uint64(-1)) const;

	uint32 SRV() const { return Texture.SRV; }
	uint64 Width() const { return Texture.Width; }
	uint64 Height() const { return Texture.Height; }
	ID3D12Resource* Resource() const { return Texture.Resource; }

private:

	DepthBuffer(const DepthBuffer& other) { }
};

}