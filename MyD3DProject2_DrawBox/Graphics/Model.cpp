#include "Model.h"
#include "..\\Utility.h"

using namespace DirectX;

namespace Framework
{

static const InputElementType StandardInputElementTypes[6] =
{
	InputElementType::Position,
	InputElementType::Normal,
	InputElementType::UV,
	InputElementType::Tangent,
	InputElementType::Bitangent,
	InputElementType::Color,
};

static const D3D12_INPUT_ELEMENT_DESC StandardInputElements[6] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

static const uint64 NumBoxVerts = 24;
static const uint64 NumBoxIndices = 36;

void Mesh::InitBox(uint32 materialIdx, MeshVertex* dstVertices, uint16* dstIndices)
{
	uint64 vIdx = 0;

	// Top
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(Colors::Green));

	// Bottom
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(Colors::Green));

	// Front
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Green));

	// Back
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Green));

	// Left
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Green));

	// Right
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(Colors::Green));

	for (uint64 i = 0; i < NumBoxVerts; ++i)
	{
		XMFLOAT3 pos = dstVertices[i].Position;
		dstVertices[i].Position = XMFLOAT3(pos.x * 0.5f, pos.y * 0.5f, pos.z * 0.5f + 0.5f);
	}

	uint64 iIdx = 0;

	// Top
	dstIndices[iIdx++] = 0;
	dstIndices[iIdx++] = 1;
	dstIndices[iIdx++] = 2;
	dstIndices[iIdx++] = 2;
	dstIndices[iIdx++] = 3;
	dstIndices[iIdx++] = 0;

	// Bottom
	dstIndices[iIdx++] = 4 + 0;
	dstIndices[iIdx++] = 4 + 1;
	dstIndices[iIdx++] = 4 + 2;
	dstIndices[iIdx++] = 4 + 2;
	dstIndices[iIdx++] = 4 + 3;
	dstIndices[iIdx++] = 4 + 0;

	// Front
	dstIndices[iIdx++] = 8 + 0;
	dstIndices[iIdx++] = 8 + 1;
	dstIndices[iIdx++] = 8 + 2;
	dstIndices[iIdx++] = 8 + 2;
	dstIndices[iIdx++] = 8 + 3;
	dstIndices[iIdx++] = 8 + 0;

	// Back
	dstIndices[iIdx++] = 12 + 0;
	dstIndices[iIdx++] = 12 + 1;
	dstIndices[iIdx++] = 12 + 2;
	dstIndices[iIdx++] = 12 + 2;
	dstIndices[iIdx++] = 12 + 3;
	dstIndices[iIdx++] = 12 + 0;

	// Left
	dstIndices[iIdx++] = 16 + 0;
	dstIndices[iIdx++] = 16 + 1;
	dstIndices[iIdx++] = 16 + 2;
	dstIndices[iIdx++] = 16 + 2;
	dstIndices[iIdx++] = 16 + 3;
	dstIndices[iIdx++] = 16 + 0;

	// Right
	dstIndices[iIdx++] = 20 + 0;
	dstIndices[iIdx++] = 20 + 1;
	dstIndices[iIdx++] = 20 + 2;
	dstIndices[iIdx++] = 20 + 2;
	dstIndices[iIdx++] = 20 + 3;
	dstIndices[iIdx++] = 20 + 0;

	const uint32 indexSize = 2;
	indexType = IndexType::Index16Bit;

	numVertices = uint32(NumBoxVerts);
	numIndices = uint32(NumBoxIndices);

	meshParts.Init(1);
	MeshPart& part = meshParts[0];
	part.IndexStart = 0;
	part.IndexCount = numIndices;
	part.VertexStart = 0;
	part.VertexCount = numVertices;
	part.MaterialIdx = materialIdx;
}

void Mesh::InitCommon(const MeshVertex* vertices_, const uint16* indices_, uint64 vbAddress, 
	uint64 ibAddress, uint64 vtxOffset_, uint64 idxOffset_)
{
	Assert_(meshParts.Size() > 0);

	vertices = vertices_;
	indices = indices_;
	vtxOffset = uint32(vtxOffset_);
	idxOffset = uint32(idxOffset_);

	vbView.BufferLocation = vbAddress;
	vbView.SizeInBytes = sizeof(MeshVertex) * numVertices;
	vbView.StrideInBytes = sizeof(MeshVertex);

	ibView.BufferLocation = ibAddress;
	ibView.SizeInBytes = IndexSize() * numIndices;
	ibView.Format = IndexBufferFormat();
}

void Mesh::Shutdown()
{
	numVertices = 0;
	numIndices = 0;
	meshParts.Shutdown();
	vertices = nullptr;
	indices = nullptr;
}

const char* Mesh::InputElementTypeString(InputElementType elemType)
{
	static const char* ElemStrings[] =
	{
		"POSITION",
		"NORMAL",
		"TANGENT",
		"BITANGENT",
		"UV",
		"COLOR",
	};

	StaticAssert_(ArraySize_(ElemStrings) == uint64(InputElementType::NumTypes));
	Assert_(uint64(elemType) < uint64(InputElementType::NumTypes));

	return ElemStrings[uint64(elemType)];
}

// == Model =======================================================================================

void Model::GenerateBox()
{
	vertices.Init(NumBoxVerts);
	indices.Init(NumBoxIndices);

	meshes.Init(1);
	meshes[0].InitBox(0, vertices.Data(), indices.Data());

	CreateBuffers();
}

void Model::Shutdown()
{
	for (uint64 i = 0; i < meshes.Size(); ++i)
		meshes[i].Shutdown();
	meshes.Shutdown();

	vertexBuffer.Shutdown();
	indexBuffer.Shutdown();
	vertices.Shutdown();
	indices.Shutdown();
}

const D3D12_INPUT_ELEMENT_DESC* Model::InputElements()
{
	return StandardInputElements;
}

const InputElementType* Model::InputElementTypes()
{
	return StandardInputElementTypes;
}

uint64 Model::NumInputElements()
{
	return ArraySize_(StandardInputElements);
}

void Model::CreateBuffers()
{
	Assert_(meshes.Size() > 0);

	StructuredBufferInit sbInit;
	sbInit.Stride = sizeof(MeshVertex);
	sbInit.NumElements = vertices.Size();
	sbInit.InitData = vertices.Data();
	vertexBuffer.Initialize(sbInit);

	FormattedBufferInit fbInit;
	fbInit.Format = DXGI_FORMAT_R16_UINT;
	fbInit.NumElements = indices.Size();
	fbInit.InitData = indices.Data();
	indexBuffer.Initialize(fbInit);

	uint64 vtxOffset = 0;
	uint64 idxOffset = 0;
	const uint64 numMeshes = meshes.Size();
	for (uint64 i = 0; i < numMeshes; ++i)
	{
		uint64 vbOffset = vtxOffset * sizeof(MeshVertex);
		uint64 ibOffset = idxOffset * sizeof(uint16);
		meshes[i].InitCommon(&vertices[vtxOffset], &indices[idxOffset], vertexBuffer.GPUAddress + vbOffset,
			indexBuffer.GPUAddress + ibOffset, vtxOffset, idxOffset);

		vtxOffset += meshes[i].NumVertices();
		idxOffset += meshes[i].NumIndices();
	}
}

}