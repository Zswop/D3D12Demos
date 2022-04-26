#include "DX12_Upload.h"
#include "DX12.h"
#include "GraphicsTypes.h"

namespace Framework
{

namespace DX12
{

struct UploadSubmission
{
	ID3D12CommandAllocator* CmdAllocator = nullptr;
	ID3D12GraphicsCommandList1* CmdList = nullptr;
	uint64 Offset = 0;
	uint64 Size = 0;
	uint64 FenceValue = 0;
	uint64 Padding = 0;

	void Reset()
	{
		Offset = 0;
		Size = 0;
		FenceValue = 0;
		Padding = 0;
	}
};

static const uint64 UploadBufferSize = 32 * 1024 * 1024;
static const uint64 MaxUploadSubmissions = 16;
static ID3D12Resource* UploadBuffer;
static uint8* UploadBufferCPUAddr = nullptr;
static SRWLOCK UploadSubmissionLock = SRWLOCK_INIT;
static SRWLOCK UploadQueueLock = SRWLOCK_INIT;

// These are protected by UploadQueueLock
static ID3D12CommandQueue* UploadCmdQueue = nullptr;
static Fence UploadFence;
static uint64 UploadFenceValue = 0;

// These are protected by UploadSubmissionLock
static uint64 UploadBufferStart = 0;
static uint64 UploadBufferUsed = 0;
static UploadSubmission UploadSubmissions[MaxUploadSubmissions];
static uint64 UploadSubmissionStart = 0;
static uint64 UploadSubmissionUsed = 0;

static const uint64 TempBufferSize = 2 * 1024 * 1024;
static ID3D12Resource* TempFrameBuffers[RenderLatency] = { };
static uint8* TempFrameCPUMem[RenderLatency] = { };
static uint64 TempFrameGPUMem[RenderLatency] = { };
static volatile int64 TempFrameUsed = 0;

static void ClearFinishedUploads(uint64 flushCount)
{
	const uint64 start = UploadSubmissionStart;
	const uint64 used = UploadSubmissionUsed;
	for (uint64 i = 0; i < used; ++i)
	{
		const uint64 idx = (start + i) % MaxUploadSubmissions;
		UploadSubmission& submission = UploadSubmissions[idx];
		Assert_(submission.Size > 0);
		Assert_(UploadBufferUsed >= submission.Size);

		if (submission.FenceValue == uint64(-1))
			return;

		if (i < flushCount)
			UploadFence.Wait(submission.FenceValue);

		if (UploadFence.Signaled(submission.FenceValue))
		{
			UploadSubmissionStart = (UploadSubmissionStart + 1) % MaxUploadSubmissions;
			UploadSubmissionUsed -= 1;
			UploadBufferStart = (UploadBufferStart + submission.Padding) % UploadBufferSize;
			Assert_(submission.Offset == UploadBufferStart);
			Assert_(UploadBufferStart + submission.Size <= UploadBufferSize);
			UploadBufferStart = (UploadBufferStart + submission.Size) % UploadBufferSize;
			UploadBufferUsed -= (submission.Size + submission.Padding);
			submission.Reset();

			if (UploadBufferUsed == 0)
				UploadBufferStart = 0;
		}
		else
		{
			return;
		}
	}
}

static UploadSubmission* AllocUploadSubmission(uint64 size)
{
	Assert_(UploadSubmissionUsed <= MaxUploadSubmissions);
	if (UploadSubmissionUsed == MaxUploadSubmissions)
		return nullptr;

	Assert_(UploadBufferUsed <= UploadBufferSize);
	if (size > (UploadBufferSize - UploadBufferUsed))
		return nullptr;

	const uint64 submissionIdx = (UploadSubmissionStart + UploadSubmissionUsed) % MaxUploadSubmissions;
	Assert_(UploadSubmissions[submissionIdx].Size == 0);

	const uint64 start = UploadBufferStart;
	const uint64 end = UploadBufferStart + UploadBufferUsed;
	uint64 allocOffset = uint64(-1);
	uint64 padding = 0;
	if (end < UploadBufferSize)
	{
		const uint64 endAmt = UploadBufferSize - end;
		if (endAmt >= size)
		{
			allocOffset = end;
		}
		else if (start >= size)
		{
			// Wrap around to the beginning
			allocOffset = 0;
			UploadBufferUsed += endAmt;
			padding = endAmt;
		}
	}
	else
	{
		const uint64 wrappedEnd = end % UploadBufferSize;
		if ((start - wrappedEnd) >= size)
			allocOffset = wrappedEnd;
	}

	if (allocOffset == uint64(-1))
		return nullptr;

	UploadSubmissionUsed += 1;
	UploadBufferUsed += size;

	UploadSubmission* submission = &UploadSubmissions[submissionIdx];
	submission->Offset = allocOffset;
	submission->Size = size;
	submission->FenceValue = uint64(-1);
	submission->Padding = padding;
	return submission;
}

void Initialize_Upload()
{
	for (uint64 i = 0; i < MaxUploadSubmissions; ++i)
	{
		UploadSubmission& submission = UploadSubmissions[i];
		DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&submission.CmdAllocator)));
		DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, submission.CmdAllocator, nullptr, IID_PPV_ARGS(&submission.CmdList)));
		DXCall(submission.CmdList->Close());
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = { };
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	DXCall(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&UploadCmdQueue)));
	UploadCmdQueue->SetName(L"Upload Copy Queue");

	UploadFenceValue = 0;
	UploadFence.Init(UploadFenceValue);

	D3D12_RESOURCE_DESC resourceDesc = { };
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = uint32(UploadBufferSize);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Alignment = 0;

	DXCall(Device->CreateCommittedResource(DX12::GetUploadHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)));

	D3D12_RANGE readRange = { };
	DXCall(UploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&UploadBufferCPUAddr)));

	// Temporary buffer memory that swaps every frame
	resourceDesc.Width = uint32(TempBufferSize);
	for (uint64 i = 0; i < RenderLatency; ++i)
	{
		DXCall(Device->CreateCommittedResource(DX12::GetUploadHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&TempFrameBuffers[i])));

		DXCall(TempFrameBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&TempFrameCPUMem[i])));
		TempFrameGPUMem[i] = TempFrameBuffers[i]->GetGPUVirtualAddress();
	}
}

void Shutdown_Upload()
{
	Release(UploadBuffer);
	Release(UploadCmdQueue);
	UploadFence.Shutdown();
	for (uint64 i = 0; i < MaxUploadSubmissions; ++i)
	{
		Release(UploadSubmissions[i].CmdAllocator);
		Release(UploadSubmissions[i].CmdList);
	}
}

void EndFrame_Upload()
{
	if (TryAcquireSRWLockExclusive(&UploadSubmissionLock))
	{
		ClearFinishedUploads(0);

		ReleaseSRWLockExclusive(&UploadSubmissionLock);
	}

	{
		AcquireSRWLockExclusive(&UploadQueueLock);

		ClearFinishedUploads(0);
		GfxQueue->Wait(UploadFence.D3DFence, UploadFenceValue);

		ReleaseSRWLockExclusive(&UploadQueueLock);
	}

	TempFrameUsed = 0;
}

UploadContext ResourceUploadBegin(uint64 size)
{
	Assert_(Device != nullptr);

	// Minimum hardware allocation size 256 bytes
	size = AlignTo(size, 512);
	Assert_(size <= UploadBufferSize);
	Assert_(size > 0);

	UploadSubmission* submission = nullptr;
	{
		AcquireSRWLockExclusive(&UploadSubmissionLock);

		ClearFinishedUploads(0);
		submission = AllocUploadSubmission(size);
		while (submission == nullptr)
		{
			ClearFinishedUploads(1);
			submission = AllocUploadSubmission(size);
		}

		ReleaseSRWLockExclusive(&UploadSubmissionLock);
	}

	DXCall(submission->CmdAllocator->Reset());
	DXCall(submission->CmdList->Reset(submission->CmdAllocator, nullptr));

	UploadContext context;
	context.CmdList = submission->CmdList;
	context.Resource = UploadBuffer;
	context.CPUAddress = UploadBufferCPUAddr + submission->Offset;
	context.ResourceOffset = submission->Offset;
	context.Submission = submission;
	return context;
}

void ResourceUploadEnd(UploadContext& context)
{
	Assert_(context.CmdList != nullptr);
	Assert_(context.Submission != nullptr);

	UploadSubmission* submission = reinterpret_cast<UploadSubmission*>(context.Submission);
	{
		AcquireSRWLockExclusive(&UploadQueueLock);

		// Finish off and execute the command list
		DXCall(submission->CmdList->Close());
		ID3D12CommandList* cmdLists[1] = { submission->CmdList };
		UploadCmdQueue->ExecuteCommandLists(1, cmdLists);

		++UploadFenceValue;
		UploadFence.Signal(UploadCmdQueue, UploadFenceValue);
		submission->FenceValue = UploadFenceValue;

		ReleaseSRWLockExclusive(&UploadQueueLock);
	}

	context = UploadContext();
}

MapResult AcquireTempBufferMem(uint64 size, uint64 alignment)
{
	uint64 allocSize = size + alignment;
	uint64 offset = InterlockedAdd64(&TempFrameUsed, allocSize) - allocSize;
	if (alignment > 0)
		offset = AlignTo(offset, alignment);
	Assert_(offset + size <= TempBufferSize);

	MapResult result;
	result.CPUAddress = TempFrameCPUMem[CurrentFrameIdx] + offset;
	result.GPUAddress = TempFrameGPUMem[CurrentFrameIdx] + offset;
	result.ResourceOffset = offset;
	result.Resource = TempFrameBuffers[CurrentFrameIdx];

	return result;
}

} // namespace DX12
} // namespace Framework
