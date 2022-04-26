#pragma once

#include "..\\PCH.h"
#include "GraphicsTypes.h"
#include "..\\Containers.h"

using namespace DirectX;

namespace Framework
{

enum class IndexType
{
	Index16Bit = 0,
	Index32Bit = 1,
};

enum class InputElementType : uint64
{
	Position = 0,
	Normal,
	Tangent,
	Bitangent,
	UV,
	Color,

	NumTypes,
};

struct MeshVertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 UV;
	XMFLOAT3 Tangent;
	XMFLOAT3 Bitangent;
	XMFLOAT4 Color;

	MeshVertex()
	{
	}

	MeshVertex(const XMFLOAT3& p, const XMFLOAT3& n, const XMFLOAT2& uv, const XMFLOAT3& t, const XMFLOAT3& b, const XMFLOAT4& c)
	{
		Position = p;
		Normal = n;
		UV = uv;
		Tangent = t;
		Bitangent = b;
		Color = c;
	}
};

struct MeshPart
{
	uint32 VertexStart;
	uint32 VertexCount;
	uint32 IndexStart;
	uint32 IndexCount;
	uint32 MaterialIdx;

	MeshPart() : VertexStart(0), VertexCount(0), IndexStart(0), IndexCount(0), MaterialIdx(0)
	{
	}
};

class Mesh
{
	friend class Model;

public:

	~Mesh()
	{
		Assert_(numVertices == 0);
	}

	void InitBox(uint32 materialIdx, MeshVertex* dstVertices, uint16* dstIndices);

	void InitCommon(const MeshVertex* vertices, const uint16* indices, uint64 vbAddress, uint64 ibAddress,
		uint64 vtxOffset, uint64 idxoffset);

	void Shutdown();

	const Array<MeshPart>& MeshParts() const { return meshParts; }
	uint64 NumMeshParts() const { return meshParts.Size(); }

	uint32 NumVertices() const { return numVertices; }
	uint32 NumIndices() const { return numIndices; }
	uint32 VertexOffset() const { return vtxOffset; }
	uint32 IndexOffset() const { return idxOffset; }

	IndexType IndexBufferType() const { return IndexType::Index16Bit; }
	DXGI_FORMAT IndexBufferFormat() const { return DXGI_FORMAT_R16_UINT; }
	uint32 IndexSize() const { return 2; }

	const MeshVertex* Vertices() const { return vertices; }
	const uint16* Indices() const { return indices; }

	const D3D12_VERTEX_BUFFER_VIEW* VBView() const { return &vbView; }
	const D3D12_INDEX_BUFFER_VIEW* IBView() const { return &ibView; }

	static const char* InputElementTypeString(InputElementType elemType);

protected:

	Array<MeshPart> meshParts;

	uint32 numVertices = 0;
	uint32 numIndices = 0;
	uint32 vtxOffset = 0;
	uint32 idxOffset = 0;

	IndexType indexType = IndexType::Index16Bit;

	const MeshVertex* vertices = nullptr;
	const uint16* indices = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	D3D12_INDEX_BUFFER_VIEW ibView = { };
};

class Model
{
public:

	~Model()
	{
		Assert_(meshes.Size() == 0);
	}

	void GenerateBox();

	void Shutdown();

	const Array<Mesh>& Meshes() const { return meshes; }
	uint64 NumMeshes() const { return meshes.Size(); }

	const StructuredBuffer& VertexBuffer() const { return vertexBuffer; }
	const FormattedBuffer& IndexBuffer() const { return indexBuffer; }

	static const D3D12_INPUT_ELEMENT_DESC* InputElements();
	static const InputElementType* InputElementTypes();
	static uint64 NumInputElements();

protected:

	void CreateBuffers();

	Array<Mesh> meshes;	

	StructuredBuffer vertexBuffer;
	FormattedBuffer indexBuffer;
	Array<MeshVertex> vertices;
	Array<uint16> indices;
};

//void MakeBoxGeometry(StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, float scale = 1.0f);

}