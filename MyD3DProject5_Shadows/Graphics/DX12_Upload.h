#pragma once

#include "..\\PCH.h"

namespace Framework
{

struct MapResult
{
	void* CPUAddress = nullptr;
	uint64 GPUAddress = 0;
	uint64 ResourceOffset = 0;
	ID3D12Resource* Resource = nullptr;
};

struct UploadContext
{
	ID3D12GraphicsCommandList1* CmdList;
	void* CPUAddress = nullptr;
	uint64 ResourceOffset = 0;
	ID3D12Resource* Resource = nullptr;
	void* Submission = nullptr;
};

struct ReadbackBuffer;
struct Texture;

namespace DX12
{

	void Initialize_Upload();
	void Shutdown_Upload();

	void EndFrame_Upload();

	UploadContext ResourceUploadBegin(uint64 size);
	void ResourceUploadEnd(UploadContext& context);

	// Temporary CPU-writable buffer memory
	MapResult AcquireTempBufferMem(uint64 size, uint64 alignment);

	void ConvertAndReadbackTexture(const Texture& texture, DXGI_FORMAT outputFormat, ReadbackBuffer& buffer);

}
}