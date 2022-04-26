#include "GraphicsTypes.h"
#include "..\\InterfacePointers.h"
#include "..\\Exceptions.h"
#include "..\\Utility.h"

namespace Framework
{
// == DescriptorHeap ==============================================================================

DescriptorHeap::~DescriptorHeap()
{
	Assert_(Heaps[0] == nullptr);
}

void DescriptorHeap::Init(uint32 numPersistent, uint32 numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible)
{
	Shutdown();

	uint32 totalNumDescriptors = numPersistent + numTemporary;
	Assert_(totalNumDescriptors > 0);

	NumPersistent = numPersistent;
	NumTemporary = numTemporary;
	HeapType = heapType;
	ShaderVisible = shaderVisible;
	if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
		ShaderVisible = false;

	NumHeaps = ShaderVisible ? 2 : 1;

	DeadList.Init(numPersistent);
	for (uint32 i = 0; i < numPersistent; ++i)
		DeadList[i] = uint32(i);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
	heapDesc.NumDescriptors = uint32(totalNumDescriptors);
	heapDesc.Type = heapType;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (ShaderVisible)
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	for (uint32 i = 0; i < NumHeaps; ++i)
	{
		DXCall(DX12::Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&Heaps[i])));
		CPUStart[i] = Heaps[i]->GetCPUDescriptorHandleForHeapStart();
		if (ShaderVisible)
			GPUStart[i] = Heaps[i]->GetGPUDescriptorHandleForHeapStart();
	}

	DescriptorSize = DX12::Device->GetDescriptorHandleIncrementSize(heapType);
}

void DescriptorHeap::Shutdown()
{
	Assert_(PersistentAllocated == 0);
	for (uint64 i = 0; i < ArraySize_(Heaps); ++i)
		DX12::Release(Heaps[i]);
}

PersistentDescriptorAlloc DescriptorHeap::AllocatePersistent()
{
	Assert_(Heaps[0] != nullptr);

	AcquireSRWLockExclusive(&Lock);

	Assert_(PersistentAllocated < NumPersistent);
	uint32 idx = DeadList[PersistentAllocated];
	++PersistentAllocated;

	ReleaseSRWLockExclusive(&Lock);

	PersistentDescriptorAlloc alloc;
	alloc.Index = idx;
	for (uint32 i = 0; i < NumHeaps; ++i)
	{
		alloc.Handles[i] = CPUStart[i];
		alloc.Handles[i].ptr += idx * DescriptorSize;
	}

	return alloc;
}

void DescriptorHeap::FreePersistent(uint32& idx)
{
	if (idx == uint32(-1))
		return;

	Assert_(idx < NumPersistent);
	Assert_(Heaps[0] != nullptr);

	AcquireSRWLockExclusive(&Lock);

	Assert_(PersistentAllocated > 0);
	DeadList[PersistentAllocated - 1] = idx;
	--PersistentAllocated;

	ReleaseSRWLockExclusive(&Lock);

	idx = uint32(-1);
}

void DescriptorHeap::FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
	Assert_(NumHeaps == 1);
	if (handle.ptr != 0)
	{
		uint32 idx = IndexFromHandle(handle);
		FreePersistent(idx);
		handle = { };
	}
}

void DescriptorHeap::FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
	Assert_(NumHeaps == 1);
	if (handle.ptr != 0)
	{
		uint32 idx = IndexFromHandle(handle);
		FreePersistent(idx);
		handle = { };
	}
}

TempDescriptorAlloc DescriptorHeap::AllocateTemporary(uint32 count)
{
	Assert_(Heaps[0] != nullptr);
	Assert_(count > 0);

	uint32 tempIdx = uint32(InterlockedAdd64(&TemporaryAllocated, count)) - count;
	Assert_(tempIdx < NumTemporary);

	uint32 finalIdx = tempIdx + NumPersistent;

	TempDescriptorAlloc alloc;
	alloc.StartCPUHandle = CPUStart[HeapIndex];
	alloc.StartCPUHandle.ptr += finalIdx * DescriptorSize;
	alloc.StartGPUHandle = GPUStart[HeapIndex];
	alloc.StartGPUHandle.ptr += finalIdx * DescriptorSize;
	alloc.StartIndex = finalIdx;

	return alloc;
}

void DescriptorHeap::EndFrame()
{
	Assert_(Heaps[0] != nullptr);
	TemporaryAllocated = 0;
	HeapIndex = (HeapIndex + 1) % NumHeaps;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32 descriptorIdx) const
{
	return CPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32 descriptorIdx) const
{
	return GPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const
{
	Assert_(Heaps[0] != nullptr);
	Assert_(heapIdx < NumHeaps);
	Assert_(descriptorIdx < TotalNumDescriptors());
	D3D12_CPU_DESCRIPTOR_HANDLE handle = CPUStart[heapIdx];
	handle.ptr += descriptorIdx * DescriptorSize;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const
{
	Assert_(Heaps[0] != nullptr);
	Assert_(heapIdx < NumHeaps);
	Assert_(descriptorIdx < TotalNumDescriptors());
	Assert_(ShaderVisible);
	D3D12_GPU_DESCRIPTOR_HANDLE handle = GPUStart[heapIdx];
	handle.ptr += descriptorIdx * DescriptorSize;
	return handle;
}

uint32 DescriptorHeap::IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	Assert_(Heaps[0] != nullptr);
	Assert_(handle.ptr >= CPUStart[HeapIndex].ptr);
	Assert_(handle.ptr < CPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
	Assert_((handle.ptr - CPUStart[HeapIndex].ptr) % DescriptorSize == 0);
	return uint32(handle.ptr - CPUStart[HeapIndex].ptr) / DescriptorSize;
}

uint32 DescriptorHeap::IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
	Assert_(Heaps[0] != nullptr);
	Assert_(handle.ptr >= GPUStart[HeapIndex].ptr);
	Assert_(handle.ptr < GPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
	Assert_((handle.ptr - GPUStart[HeapIndex].ptr) % DescriptorSize == 0);
	return uint32(handle.ptr - GPUStart[HeapIndex].ptr) / DescriptorSize;
}

ID3D12DescriptorHeap* DescriptorHeap::CurrentHeap() const
{
	Assert_(Heaps[0] != nullptr);
	return Heaps[HeapIndex];
}

// == Fence =======================================================================================

Fence::~Fence()
{
	Assert_(D3DFence == nullptr);
	Shutdown();
}

void Fence::Init(uint64 initialValue)
{
	DXCall(DX12::Device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&D3DFence)));
	FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	Win32Call(FenceEvent != 0);
}

void Fence::Shutdown()
{
	DX12::DeferredRelease(D3DFence);
}

void Fence::Signal(ID3D12CommandQueue* queue, uint64 fenceValue)
{
	Assert_(D3DFence != nullptr);
	DXCall(queue->Signal(D3DFence, fenceValue));
}

void Fence::Wait(uint64 fenceValue)
{
	Assert_(D3DFence != nullptr);
	if (D3DFence->GetCompletedValue() < fenceValue)
	{
		DXCall(D3DFence->SetEventOnCompletion(fenceValue, FenceEvent));
		WaitForSingleObject(FenceEvent, INFINITE);
	}
}

bool Fence::Signaled(uint64 fenceValue)
{
	Assert_(D3DFence != nullptr);
	return D3DFence->GetCompletedValue() >= fenceValue;
}

void Fence::Clear(uint64 fenceValue)
{
	Assert_(D3DFence != nullptr);
	D3DFence->Signal(fenceValue);
}

// == Texture ====================================================================================

Texture::Texture()
{
}

Texture::~Texture()
{
	Assert_(Resource == nullptr);
}

void Texture::Shutdown()
{
	DX12::SRVDescriptorHeap.FreePersistent(SRV);
	DX12::DeferredRelease(Resource);
}

// == RenderTexture =============================================================================

RenderTexture::RenderTexture()
{
}

RenderTexture::~RenderTexture()
{
	Assert_(RTV.ptr == 0);
}

void RenderTexture::Initialize(const RenderTextureInit& init)
{
	Shutdown();

	Assert_(init.Width > 0);
	Assert_(init.Height > 0);
	Assert_(init.MSAASamples > 0);

	D3D12_RESOURCE_DESC textureDesc = { };
	textureDesc.MipLevels = 1;
	textureDesc.Format = init.Format;
	textureDesc.Width = uint32(init.Width);
	textureDesc.Height = uint32(init.Height);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (init.CreateUAV)
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureDesc.DepthOrArraySize = uint16(init.ArraySize);
	textureDesc.SampleDesc.Count = uint32(init.MSAASamples);
	textureDesc.SampleDesc.Quality = init.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Alignment = 0;

	D3D12_CLEAR_VALUE clearValue = { };
	clearValue.Format = init.Format;
	DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
		init.InitialState, &clearValue, IID_PPV_ARGS(&Texture.Resource)));

	if (init.Name != nullptr)
		Texture.Resource->SetName(init.Name);

	PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
	Texture.SRV = srvAlloc.Index;
	for (uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
		DX12::Device->CreateShaderResourceView(Texture.Resource, nullptr, srvAlloc.Handles[i]);

	Texture.Width = uint32(init.Width);
	Texture.Height = uint32(init.Height);
	Texture.Depth = 1;
	Texture.NumMips = 1;
	Texture.ArraySize = uint32(init.ArraySize);
	Texture.Format = init.Format;
	Texture.Cubemap = false;
	MSAASamples = uint32(init.MSAASamples);
	MSAAQuality = uint32(textureDesc.SampleDesc.Quality);

	RTV = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
	DX12::Device->CreateRenderTargetView(Texture.Resource, nullptr, RTV);

	if (init.ArraySize > 1)
	{
		ArrayRTVs.Init(init.ArraySize);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = init.Format;
		if (init.MSAASamples > 1)
			rtvDesc.Texture2DMSArray.ArraySize = 1;
		else
			rtvDesc.Texture2DArray.ArraySize = 1;

		for (uint64 i = 0; i < init.ArraySize; ++i)
		{
			if (init.MSAASamples > 1)
				rtvDesc.Texture2DMSArray.FirstArraySlice = uint32(i);
			else
				rtvDesc.Texture2DArray.FirstArraySlice = uint32(i);

			ArrayRTVs[i] = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
			DX12::Device->CreateRenderTargetView(Texture.Resource, &rtvDesc, ArrayRTVs[i]);
		}
	}

	if (init.CreateUAV)
	{
		UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];
		DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, nullptr, UAV);
	}
}

void RenderTexture::Shutdown()
{
	DX12::RTVDescriptorHeap.FreePersistent(RTV);
	DX12::RTVDescriptorHeap.FreePersistent(UAV);
	for (uint64 i = 0; i < ArrayRTVs.Size(); ++i)
		DX12::RTVDescriptorHeap.FreePersistent(ArrayRTVs[i]);
	ArrayRTVs.Shutdown();
	Texture.Shutdown();
}

void RenderTexture::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, 
	D3D12_RESOURCE_STATES after, uint64 mipLevel, uint64 arraySlice) const
{
	uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
		: uint32(SubResourceIndex(mipLevel, arraySlice));

	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, before, after, subResourceIdx);
}

void RenderTexture::MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
	uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
		: uint32(SubResourceIndex(mipLevel, arraySlice));
	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subResourceIdx);
}

void RenderTexture::MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
	uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
		: uint32(SubResourceIndex(mipLevel, arraySlice));
	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, subResourceIdx);
}

void RenderTexture::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
	Assert_(Texture.Resource != nullptr);

	D3D12_RESOURCE_BARRIER barrier = { };
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = Texture.Resource;
	cmdList->ResourceBarrier(1, &barrier);
}

// == DepthBuffer ===============================================================================

DepthBuffer::DepthBuffer()
{
}

DepthBuffer::~DepthBuffer()
{
	Assert_(DSVFormat == DXGI_FORMAT_UNKNOWN);
	Shutdown();
}

void DepthBuffer::Initialize(const DepthBufferInit& init)
{
	Shutdown();

	Assert_(init.Width > 0);
	Assert_(init.Height > 0);
	Assert_(init.MSAASamples > 0);

	DXGI_FORMAT texFormat = init.Format;
	DXGI_FORMAT srvFormat = init.Format;
	if (init.Format == DXGI_FORMAT_D16_UNORM)
	{
		texFormat = DXGI_FORMAT_R16_TYPELESS;
		srvFormat = DXGI_FORMAT_R16_UNORM;
	}
	else if (init.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		texFormat = DXGI_FORMAT_R24G8_TYPELESS;
		srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	else if (init.Format == DXGI_FORMAT_D32_FLOAT)
	{
		texFormat = DXGI_FORMAT_R32_TYPELESS;
		srvFormat = DXGI_FORMAT_R32_FLOAT;
	}
	else if (init.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
	{
		texFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	}
	else
	{
		AssertFail_("Invalid depth buffer format!");
	}

	D3D12_RESOURCE_DESC textureDesc = { };
	textureDesc.MipLevels = 1;
	textureDesc.Format = texFormat;
	textureDesc.Width = uint32(init.Width);
	textureDesc.Height = uint32(init.Height);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	textureDesc.DepthOrArraySize = uint16(init.ArraySize);
	textureDesc.SampleDesc.Count = uint32(init.MSAASamples);
	textureDesc.SampleDesc.Quality = init.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Alignment = 0;

	D3D12_CLEAR_VALUE clearValue = { };
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;
	clearValue.Format = init.Format;

	DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
		init.InitialState, &clearValue, IID_PPV_ARGS(&Texture.Resource)));

	if(init.Name != nullptr)
		Texture.Resource->SetName(init.Name);

	PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
	Texture.SRV = srvAlloc.Index;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	srvDesc.Format = srvFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (init.MSAASamples == 1 && init.ArraySize == 1)
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	}
	else if (init.MSAASamples == 1 && init.ArraySize > 1)
	{
		srvDesc.Texture2DArray.ArraySize = uint32(init.ArraySize);
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	}
	else if (init.MSAASamples > 1 && init.ArraySize == 1)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	else if (init.MSAASamples > 1 && init.ArraySize > 1)
	{
		srvDesc.Texture2DMSArray.FirstArraySlice = 0;
		srvDesc.Texture2DMSArray.ArraySize = uint32(init.ArraySize);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
	}

	for (uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
		DX12::Device->CreateShaderResourceView(Texture.Resource, &srvDesc, srvAlloc.Handles[i]);

	Texture.Width = uint32(init.Width);
	Texture.Height = uint32(init.Height);
	Texture.Depth = 1;
	Texture.NumMips = 1;
	Texture.ArraySize = uint32(init.ArraySize);
	Texture.Format = srvFormat;
	Texture.Cubemap = false;
	MSAASamples = uint32(init.MSAASamples);
	MSAAQuality = uint32(textureDesc.SampleDesc.Quality);

	DSV = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { };
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = init.Format;

	if (init.MSAASamples == 1 && init.ArraySize == 1)
	{
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	}
	else if (init.MSAASamples == 1 && init.ArraySize > 1)
	{
		dsvDesc.Texture2DArray.ArraySize = uint32(init.ArraySize);
		dsvDesc.Texture2DArray.FirstArraySlice = 0;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	}
	else if (init.MSAASamples > 1 && init.ArraySize == 1)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	}
	else if (init.MSAASamples > 1 && init.ArraySize > 1)
	{
		dsvDesc.Texture2DMSArray.ArraySize = uint32(init.ArraySize);
		dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
	}

	DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, DSV);

	bool hasStencil = init.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || init.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	ReadOnlyDSV = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];
	dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
	if (hasStencil)
		dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
	DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, ReadOnlyDSV);

	if (init.ArraySize > 1)
	{
		ArrayDSVs.Init(init.ArraySize);

		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		if (init.MSAASamples > 1)
			dsvDesc.Texture2DMSArray.ArraySize = 1;
		else
			dsvDesc.Texture2DArray.ArraySize = 1;

		for (uint64 i = 0; i < init.ArraySize; ++i)
		{
			if (init.MSAASamples > 1)
				dsvDesc.Texture2DMSArray.FirstArraySlice = uint32(i);
			else
				dsvDesc.Texture2DArray.FirstArraySlice = uint32(i);

			ArrayDSVs[i] = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];
			DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, ArrayDSVs[i]);
		}
	}

	DSVFormat = init.Format;
}

void DepthBuffer::Shutdown()
{
	DX12::DSVDescriptorHeap.FreePersistent(DSV);
	DX12::DSVDescriptorHeap.FreePersistent(ReadOnlyDSV);
	for (uint64 i = 0; i < ArrayDSVs.Size(); ++i)
		DX12::DSVDescriptorHeap.FreePersistent(ArrayDSVs[i]);
	ArrayDSVs.Shutdown();
	Texture.Shutdown();
	DSVFormat = DXGI_FORMAT_UNKNOWN;
}

void DepthBuffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint64 arraySlice) const
{
	uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, before, after, subResourceIdx);
}

void DepthBuffer::MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice) const
{
	uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subResourceIdx);
}

void DepthBuffer::MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice) const
{
	uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
	Assert_(Texture.Resource != nullptr);
	DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, subResourceIdx);
}

}