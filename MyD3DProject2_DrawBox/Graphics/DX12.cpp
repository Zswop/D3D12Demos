#include "DX12.h"
#include "DX12_Helpers.h"
#include "DX12_Upload.h"
#include "GraphicsTypes.h"
#include "..\\Exceptions.h"
#include "..\\Utility.h"

#if _DEBUG
	#define UseDebugDevice_ 1
	#define BreakOnDXError_ (UseDebugDevice_ && 1)
	#define UseGPUValidation_ 0
#else
	#define UseDebugDevice_ 0
	#define BreakOnDXError_ 0
	#define UseGPUValidation_ 0
#endif

namespace Framework
{

namespace DX12
{

ID3D12Device2* Device = nullptr;
ID3D12GraphicsCommandList1* CmdList = nullptr;
ID3D12CommandQueue* GfxQueue = nullptr;
D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
IDXGIFactory4* Factory = nullptr;
IDXGIAdapter1* Adapter = nullptr;

uint64 CurrentCPUFrame = 0;
uint64 CurrentGPUFrame = 0;
uint64 CurrentFrameIdx = 0;

static const uint64 NumCmdAllocators = RenderLatency;
static ID3D12CommandAllocator* CmdAllocators[NumCmdAllocators] = { };
static Fence FrameFence;

static GrowableList<IUnknown*> DeferredReleases[RenderLatency];
static bool ShuttingDown = false;

struct DeferredSRVCreate
{
	ID3D12Resource* Resource = nullptr;
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc = { };
	uint32 DescriptorIdx = uint32(-1);
};

static Array<DeferredSRVCreate> DeferredSRVCreates[RenderLatency];
static volatile uint64 DeferredSRVCreateCount[RenderLatency] = { };

static void ProcessDeferredReleases(uint64 frameIdx)
{
	for (uint64 i = 0; i < DeferredReleases[frameIdx].Count(); ++i)
		DeferredReleases[frameIdx][i]->Release();
	DeferredReleases[frameIdx].RemoveAll(nullptr);
}

static void ProcessDeferredSRVCreates(uint64 frameIdx)
{
	uint64 createCount = DeferredSRVCreateCount[frameIdx];
	for (uint64 i = 0; i < createCount; ++i)
	{
		DeferredSRVCreate& create = DeferredSRVCreates[frameIdx][i];
		Assert_(create.Resource != nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = SRVDescriptorHeap.CPUHandleFromIndex(create.DescriptorIdx, frameIdx);
		Device->CreateShaderResourceView(create.Resource, &create.Desc, handle);

		create.Resource = nullptr;
		create.DescriptorIdx = uint32(-1);
	}

	DeferredSRVCreateCount[frameIdx] = 0;
}

void Initialize(D3D_FEATURE_LEVEL minFeatureLevel, uint32 adapterIdx)
{
	ShuttingDown = false;

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&Factory));
	if (FAILED(hr))
		throw Exception(L"Unable to create a DXGI 1.4 device.\n "
			L"Make sure that your OS and driver support DirectX 12");

	LARGE_INTEGER umdVersion = { };
	Factory->EnumAdapters1(adapterIdx, &Adapter);
	if (Adapter == nullptr)
		throw Exception(L"Unable to locate a DXGI 1.4 adapter that supports a D3D12 device.\n"
			L"Make sure that your OS and driver support DirectX 12");

	DXGI_ADAPTER_DESC1 desc = { };
	Adapter->GetDesc1(&desc);
	WriteLog(L"Creating DX12 device on adapter '%ls'", desc.Description);

	#if UseDebugDevice_
		ID3D12DebugPtr d3d12debug;
		DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12debug)));
		d3d12debug->EnableDebugLayer();

		#if UseGPUValidation_
			ID3D12Debug1Ptr debug1;
			d3d12debug->QueryInterface(IID_PPV_ARGS(&debug1));
			debug1->SetEnableGPUBasedValidation(true);
		#endif
	#endif

	#if EnableShaderModel6_ && 0
		// Enable experimental shader models
		DXCall(D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr));
	#endif

	DXCall(D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device)));

	// Check the maximum feature level, and make sure it's above our minimum
	D3D_FEATURE_LEVEL featureLevelsArray[4];
	featureLevelsArray[0] = D3D_FEATURE_LEVEL_11_0;
	featureLevelsArray[1] = D3D_FEATURE_LEVEL_11_1;
	featureLevelsArray[2] = D3D_FEATURE_LEVEL_12_0;
	featureLevelsArray[3] = D3D_FEATURE_LEVEL_12_1;
	D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = { };
	featureLevels.NumFeatureLevels = ArraySize_(featureLevelsArray);
	featureLevels.pFeatureLevelsRequested = featureLevelsArray;
	DXCall(Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)));
	FeatureLevel = featureLevels.MaxSupportedFeatureLevel;

	if (FeatureLevel < minFeatureLevel)
	{
		std::wstring majorLevel = ToString<int>(minFeatureLevel >> 12);
		std::wstring minorLevel = ToString<int>((minFeatureLevel >> 8) & 0xF);
		throw Exception(L"The device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
	}

	#if UseDebugDevice_
		ID3D12InfoQueuePtr infoQueue;
		DXCall(Device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

		D3D12_MESSAGE_ID disabledMessages[] =
		{
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,

			// These happen when capturing with VS diagnostics
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
		};

		D3D12_INFO_QUEUE_FILTER filter = { };
		filter.DenyList.NumIDs = ArraySize_(disabledMessages);
		filter.DenyList.pIDList = disabledMessages;
		infoQueue->AddStorageFilterEntries(&filter);
	#endif

	#if BreakOnDXError_
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	#endif

	for (uint64 i = 0; i < NumCmdAllocators; ++i)
		DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAllocators[i])));

	DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAllocators[0], nullptr, IID_PPV_ARGS(&CmdList)));
	DXCall(CmdList->Close());
	CmdList->SetName(L"Primary Graphics Command List");

	D3D12_COMMAND_QUEUE_DESC queueDesc = { };
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXCall(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&GfxQueue)));
	GfxQueue->SetName(L"Main Gfx Queue");

	CurrentFrameIdx = CurrentCPUFrame % NumCmdAllocators;
	DXCall(CmdAllocators[CurrentFrameIdx]->Reset());
	DXCall(CmdList->Reset(CmdAllocators[CurrentFrameIdx], nullptr));

	FrameFence.Init(0);

	for (uint64 i = 0; i < ArraySize_(DeferredSRVCreates); ++i)
		DeferredSRVCreates[i].Init(1024);

	Initialize_Helpers();
	Initialize_Upload();
}

void Shutdown()
{
	Assert_(CurrentCPUFrame == CurrentGPUFrame);
	ShuttingDown = true;

	for (uint64 i = 0; i < ArraySize_(DeferredReleases); ++i)
		ProcessDeferredReleases(i);

	FrameFence.Shutdown();

	for (uint64 i = 0; i < RenderLatency; ++i)
		Release(CmdAllocators[i]);

	Release(CmdList);
	Release(GfxQueue);
	Release(Factory);
	Release(Adapter);
	
	Shutdown_Helpers();
	Shutdown_Upload();

#if BreakOnDXError_
	if (Device != nullptr)
	{
		ID3D12InfoQueuePtr infoQueue;
		DXCall(Device->QueryInterface(IID_PPV_ARGS(&infoQueue)));
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
	}
#endif


#if UseDebugDevice_ && 0
	if (Device != nullptr)
	{
		ID3D12DebugDevicePtr debugDevice;
		DXCall(Device->QueryInterface(IID_PPV_ARGS(&debugDevice)));
		debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
	}
#endif

	Release(Device);
}

void BeginFrame()
{
	Assert_(Device);

	SetDescriptorHeaps(CmdList);
}

void EndFrame(IDXGISwapChain4* swapChain, uint32 syncIntervals)
{
	Assert_(Device);

	DXCall(CmdList->Close());

	EndFrame_Upload();

	ID3D12CommandList* commandLists[] = { CmdList };
	GfxQueue->ExecuteCommandLists(ArraySize_(commandLists), commandLists);

	// Present the frame.
	if (swapChain)
		DXCall(swapChain->Present(syncIntervals, syncIntervals == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0));

	++CurrentCPUFrame;

	// Signal the fence with the current frame number, so that we can check back on it
	FrameFence.Signal(GfxQueue, CurrentCPUFrame);

	// Wait for the GPU to catch up before we stomp an executing command buffer
	const uint64 gpuLag = DX12::CurrentCPUFrame - DX12::CurrentGPUFrame;
	Assert_(gpuLag <= DX12::RenderLatency);
	if (gpuLag >= DX12::RenderLatency)
	{
		// Make sure that the previous frame is finished
		FrameFence.Wait(DX12::CurrentGPUFrame + 1);
		++DX12::CurrentGPUFrame;
	}

	CurrentFrameIdx = DX12::CurrentCPUFrame % NumCmdAllocators;

	// Prepare the command buffers to be used for the next frame
	DXCall(CmdAllocators[CurrentFrameIdx]->Reset());
	DXCall(CmdList->Reset(CmdAllocators[CurrentFrameIdx], nullptr));

	EndFrame_Helpers();

	// See if we have any deferred releases to process
	ProcessDeferredReleases(CurrentFrameIdx);

	ProcessDeferredSRVCreates(CurrentFrameIdx);
}

void FlushGPU()
{
	Assert_(Device);

	//Wait for the GPU to fully catch up with the CPU
	Assert_(CurrentCPUFrame >= CurrentGPUFrame);
	if (CurrentCPUFrame > CurrentGPUFrame)
	{
		FrameFence.Wait(CurrentCPUFrame);
		CurrentGPUFrame = CurrentCPUFrame;
	}

	// Clean up what we can now
	for (uint64 i = 1; i < RenderLatency; ++i)
	{
		uint64 frameIdx = (i + CurrentFrameIdx) % RenderLatency;
		ProcessDeferredReleases(frameIdx);
		ProcessDeferredSRVCreates(frameIdx);
	}
}

void DeferredRelease_(IUnknown* resource)
{
	if (resource == nullptr)
		return;

	if (ShuttingDown || Device == nullptr)
	{
		// Free-for-all!
		resource->Release();
		return;
	}

	DeferredReleases[CurrentFrameIdx].Add(resource);
}

void DeferredCreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, uint32 descriptorIdx)
{
	for (uint64 i = 1; i < RenderLatency; ++i)
	{
		uint64 frameIdx = (CurrentCPUFrame + i) % RenderLatency;
		uint64 writeIdx = InterlockedIncrement(&DeferredSRVCreateCount[frameIdx]) - 1;
		DeferredSRVCreate& create = DeferredSRVCreates[frameIdx][writeIdx];
		create.Resource = resource;
		create.Desc = desc;
		create.DescriptorIdx = descriptorIdx;
	}
}

} // namespace DX12
} // namespace Framework