#include "Texture.h"

#include "..\\Exceptions.h"
#include "..\\FileIO.h"

namespace Framework
{

// Return the number of mip levels given a texture size
static uint64 NumMipLevels(uint64 width, uint64 height, uint64 depth = 1)
{
	uint64 numMips = 0;
	uint64 size = std::max(std::max(width, height), depth);
	while ((1ull << numMips) <= size)
		++numMips;

	if ((1ull << numMips) < size)
		++numMips;

	return numMips;
}

void LoadTexture(Texture& texture, const wchar* filePath, bool forceSRGB)
{
	texture.Shutdown();
	if (FileExists(filePath) == false)
		throw Exception(MakeString(L"Texture file with path '%ls' does not exist", filePath));

	DirectX::ScratchImage image;

	const std::wstring extension = GetFileExtension(filePath);
	if (extension == L"DDS" || extension == L"dds") 
	{
		DXCall(DirectX::LoadFromDDSFile(filePath, DirectX::DDS_FLAGS_NONE, nullptr, image));
	}
	else if (extension == L"TGA" || extension == L"tga")
	{
		DirectX::ScratchImage tempImage;
		DXCall(DirectX::LoadFromTGAFile(filePath, nullptr, tempImage));
		DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
	}
	else
	{
		DirectX::ScratchImage tempImage;
		DXCall(DirectX::LoadFromWICFile(filePath, DirectX::WIC_FLAGS_NONE, nullptr, tempImage));
		DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
	}

	const DirectX::TexMetadata& metaData = image.GetMetadata();
	DXGI_FORMAT format = metaData.format;
	if (forceSRGB)
		format = DirectX::MakeSRGB(format);

	const bool is3D = metaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

	D3D12_RESOURCE_DESC textureDesc = { };
	textureDesc.MipLevels = uint16(metaData.mipLevels);
	textureDesc.Format = format;
	textureDesc.Width = uint32(metaData.width);
	textureDesc.Height = uint32(metaData.height);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = is3D ? uint16(metaData.depth) : uint16(metaData.arraySize);
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = is3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Alignment = 0;

	ID3D12Device* device = DX12::Device;
	DXCall(device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture.Resource)));

	texture.Resource->SetName(filePath);

	PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
	texture.SRV = srvAlloc.Index;

	const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPtr = nullptr;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	if (metaData.IsCubemap())
	{
		Assert_(metaData.arraySize == 6);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = uint32(metaData.mipLevels);
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		srvDescPtr = &srvDesc;
	}

	for (uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
		device->CreateShaderResourceView(texture.Resource, srvDescPtr, srvAlloc.Handles[i]);

	const uint64 numSubResources = metaData.mipLevels * metaData.arraySize;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * numSubResources);
	uint32* numRows = (uint32*)_alloca(sizeof(uint32) * numSubResources);
	uint64* rowSizes = (uint64*)_alloca(sizeof(uint64) * numSubResources);

	uint64 textureMemSize = 0;
	device->GetCopyableFootprints(&textureDesc, 0, uint32(numSubResources), 0, layouts, numRows, rowSizes, &textureMemSize);

	// Get a GPU upload buffer
	UploadContext uploadContext = DX12::ResourceUploadBegin(textureMemSize);
	uint8* uploadMem = reinterpret_cast<uint8*>(uploadContext.CPUAddress);

	for (uint64 arrayIdx = 0; arrayIdx < metaData.arraySize; ++arrayIdx)
	{
		for (uint64 mipIdx = 0; mipIdx < metaData.mipLevels; ++mipIdx)
		{
			const uint64 subResourceIdx = mipIdx + (arrayIdx * metaData.mipLevels);

			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIdx];
			const uint64 subResourceHeight = numRows[subResourceIdx];
			const uint64 subResourcePitch = subResourceLayout.Footprint.RowPitch;
			const uint64 subResourceDepth = subResourceLayout.Footprint.Depth;

			uint8* dstSubResourceMem = reinterpret_cast<uint8*>(uploadMem) + subResourceLayout.Offset;
			for (uint64 z = 0; z < subResourceDepth; ++z)
			{
				const DirectX::Image* subImage = image.GetImage(mipIdx, arrayIdx, z);
				Assert_(subImage != nullptr);
				const uint8* srcSubResourceMem = subImage->pixels;

				for (uint64 y = 0; y < subResourceHeight; ++y)
				{
					memcpy(dstSubResourceMem, srcSubResourceMem, Min(subResourcePitch, subImage->rowPitch));
					dstSubResourceMem += subResourcePitch;
					srcSubResourceMem += subImage->rowPitch;
				}
			}
		}
	}

	for (uint64 subResourceIdx = 0; subResourceIdx < numSubResources; ++subResourceIdx)
	{
		D3D12_TEXTURE_COPY_LOCATION dst = { };
		dst.pResource = texture.Resource;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = uint32(subResourceIdx);
		D3D12_TEXTURE_COPY_LOCATION src = { };
		src.pResource = uploadContext.Resource;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = layouts[subResourceIdx];
		src.PlacedFootprint.Offset += uploadContext.ResourceOffset;
		uploadContext.CmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	DX12::ResourceUploadEnd(uploadContext);

	texture.Width = uint32(metaData.width);
	texture.Height = uint32(metaData.height);
	texture.Depth = uint32(metaData.depth);
	texture.NumMips = uint32(metaData.mipLevels);
	texture.ArraySize = uint32(metaData.arraySize);
	texture.Format = metaData.format;
	texture.Cubemap = metaData.IsCubemap() ? 1 : 0;
}

}
