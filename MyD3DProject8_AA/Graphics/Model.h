#pragma once

#include "..\\PCH.h"
#include "GraphicsTypes.h"
#include "..\\Containers.h"
#include "..\\F12_Math.h"

using namespace DirectX;

namespace Framework
{
static const uint64 MaxSpotLights = 32;

struct MeshVertex
{
	Float3 Position;
	Float3 Normal;
	Float2 UV;
	Float3 Tangent;
	Float3 Bitangent;
	Float4 Color;

	MeshVertex()
	{
	}

	MeshVertex(const Float3& p, const Float3& n, const Float2& uv, const Float3& t, const Float3& b, const Float4& c = Float4(1.0f))
	{
		Position = p;
		Normal = n;
		UV = uv;
		Tangent = t;
		Bitangent = b;
		Color = c;
	}

	void Transform(const Float3& p, const Float3& s, const Quaternion& q)
	{
		Position *= s;
		Position = Float3::Transform(Position, q);
		Position += p;

		Normal = Float3::Transform(Normal, q);
		Tangent = Float3::Transform(Tangent, q);
		Bitangent = Float3::Transform(Bitangent, q);
	}
};

enum class MaterialTextures : uint64
{
	Albedo = 0,
	Normal,
	Roughness,
	Metallic,
	Opacity,
	Emissive,

	Count,
};

struct MeshMaterial
{
	std::wstring TextureNames[uint64(MaterialTextures::Count)];
	const Texture* Textures[uint64(MaterialTextures::Count)];
	uint32 TextureIndices[uint64(MaterialTextures::Count)];

	uint32 Texture(MaterialTextures texType) const
	{
		Assert_(uint64(texType) < uint64(MaterialTextures::Count));
		Assert_(Textures[uint64(texType)] != nullptr);
		return Textures[uint64(texType)]->SRV;
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

struct MaterialTexture
{
	std::wstring Name;
	Texture Texture;
};

struct ModelSpotLight
{
	Float3 Position;
	Float3 Intensity;
	Float3 Direction;
	Quaternion Orientation;
	Float2 AngularAttenuation;
};

struct SpotLight
{
	Float3 Position;
	float AngularAttenuationX;
	Float3 Direction;
	float AngularAttenuationY;
	Float3 Intensity;
	float Range;
};

struct PointLight
{
	Float3 Position;
	Float3 Intensity;
};

class Mesh
{
	friend class Model;

public:

	~Mesh()
	{
		Assert_(numVertices == 0);
	}

	void InitFromAssimpMesh(const aiMesh& assimpMesh, float sceneScale, MeshVertex* dstVertices, uint16* dstIndices);

	void InitBox(const Float3& position, const Float3& dimensions, const Quaternion& orientation,
		uint32 materialIdx, MeshVertex* dstVertices, uint16* dstIndices);

	void InitPlane(const Float3& position, const Float2& dimensions, const Quaternion& orientation,
		uint32 materialIdx,	MeshVertex* dstVertices, uint16* dstIndices);

	void InitSphere(const Float3& position, const float scale, const Quaternion& orientation, uint32 materialIdx,
		 MeshVertex* dstVertices, uint16* dstIndices, uint64 uDivisions, uint64 vDivisions);

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

	const Float3& AABBMin() const { return aabbMin; }
	const Float3& AABBMax() const { return aabbMax; }

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

	Float3 aabbMin;
	Float3 aabbMax;
};

struct ModelLoadSettings
{
	const wchar* FilePath = nullptr;
	float SceneScale = 1.0f;
	bool ForceSRGB = false;
	bool MergeMeshes = true;
};

class Model
{
public:

	~Model()
	{
		Assert_(meshes.Size() == 0);
	}

	// Loading from file formats
	void CreateWithAssimp(const ModelLoadSettings& settings);

	void GenerateBox(const Float3& position, const Float3& dimensions, const Quaternion& orientation,
		const wchar* colorMap, const wchar* normalMap, const wchar* roughnessMap);

	void GenerateTestScene();

	void Shutdown();

	const Array<Mesh>& Meshes() const { return meshes; }
	uint64 NumMeshes() const { return meshes.Size(); }

	const Float3& AABBMin() const { return aabbMin; }
	const Float3& AABBMax() const { return aabbMax; }

	const Array<MeshMaterial>& Materials() const { return meshMaterials; }
	const GrowableList<MaterialTexture*>& MaterialTextures() const { return materialTextures; }

	const Array<ModelSpotLight>& SpotLights() const { return spotLights; }
	const Array<PointLight>& PointLights() const { return pointLights; }

	const StructuredBuffer& VertexBuffer() const { return vertexBuffer; }
	const FormattedBuffer& IndexBuffer() const { return indexBuffer; }

	static const D3D12_INPUT_ELEMENT_DESC* InputElements();
	static const InputElementType* InputElementTypes();
	static uint64 NumInputElements();

protected:

	void CreateBuffers();

	Array<Mesh> meshes;	
	Array<MeshMaterial> meshMaterials;	
	Array<ModelSpotLight> spotLights;
	Array<PointLight> pointLights;
	std::wstring fileDirectory;
	bool32 forceSRGB = false;
	Float3 aabbMin;
	Float3 aabbMax;

	StructuredBuffer vertexBuffer;
	FormattedBuffer indexBuffer;
	Array<MeshVertex> vertices;
	Array<uint16> indices;

	GrowableList<MaterialTexture*> materialTextures;
};

// void MakeSphereGeometry(uint64 uDivisions, uint64 vDivisions, MeshVertex* dstVertices, uint16* dstIndices);
// void MakeBoxGeometry(MeshVertex* dstVertices, uint16* dstIndices);

void MakeConeGeometry(uint64 divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, Array<Float3>& positions);
void MakeConeGeometry(uint64 divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer);
}