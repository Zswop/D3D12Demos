#include "Model.h"
#include "..\\Utility.h"
#include "..\\FileIO.h"
#include "Texture.h"

using namespace DirectX;

using std::wstring;

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

static const wchar* DefaultTextures[] =
{
	L"Content\\Textures\\Default.dds",				// Albedo
	L"Content\\Textures\\DefaultNormalMap.dds",		// Normal
	L"Content\\Textures\\DefaultRoughness.dds",		// Roughness
	L"Content\\Textures\\DefaultBlack.dds",			// Metallic
};

StaticAssert_(ArraySize_(DefaultTextures) == uint64(MaterialTextureType::Count));

void LoadMaterialResources(Array<MeshMaterial>& materials, const wstring& directory, bool32 forceSRGB,
	GrowableList<MaterialTexture*>& materialTextures)
{
	const uint64 numMaterials = materials.Size();
	for (uint64 matIdx = 0; matIdx < numMaterials; ++matIdx)
	{
		MeshMaterial& material = materials[matIdx];
		for (uint64 texType = 0; texType < uint64(MaterialTextureType::Count); ++texType)
		{
			material.Textures[texType] = nullptr;

			wstring path = directory + material.TextureNames[texType];
			if (material.TextureNames[texType].length() == 0 || FileExists(path.c_str()) == false)
				path = DefaultTextures[texType];

			const uint64 numLoaded = materialTextures.Count();
			for (uint64 i = 0; i < numLoaded; ++i)
			{
				if (materialTextures[i]->Name == path)
				{
					material.Textures[texType] = &materialTextures[i]->Texture;
					material.TextureIndices[texType] = uint32(i);
					break;
				}
			}

			if (material.Textures[texType] == nullptr)
			{
				MaterialTexture* newMatTexture = new MaterialTexture();
				newMatTexture->Name = path;
				bool useSRGB = forceSRGB && texType == uint64(MaterialTextureType::Albedo);
				LoadTexture(newMatTexture->Texture, path.c_str(), useSRGB ? true : false);
				uint64 idx = materialTextures.Add(newMatTexture);

				material.Textures[texType] = &newMatTexture->Texture;
				material.TextureIndices[texType] = uint32(idx);
			}
		}
	}
}

static const uint64 NumBoxVerts = 24;
static const uint64 NumBoxIndices = 36;

void Mesh::InitBox(const Float3& position, const Float3& dimensions, const Quaternion& orientation,
	uint32 materialIdx, MeshVertex* dstVertices, uint16* dstIndices)
{
	uint64 vIdx = 0;

	// Top
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float4(Colors::Green));

	// Bottom
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float4(Colors::Green));

	// Front
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Purple));

	// Back
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Blue));

	// Left
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Gold));

	// Right
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::White));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Black));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Red));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float4(Colors::Green));

	for (uint64 i = 0; i < NumBoxVerts; ++i)
		dstVertices[i].Transform(position, dimensions * 0.5f, orientation);

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

static const uint64 NumPlaneVerts = 4;
static const uint64 NumPlaneIndices = 6;

void Mesh::InitPlane(const Float3& position, const Float2& dimensions, const Quaternion& orientation,
	uint32 materialIdx, MeshVertex* dstVertices, uint16* dstIndices)
{
	uint64 vIdx = 0;

	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
	dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
	dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));

	for (uint64 i = 0; i < NumPlaneVerts; ++i)
		dstVertices[i].Transform(position, Float3(dimensions.x, 1.0f, dimensions.y) * 0.5f, orientation);

	uint64 iIdx = 0;
	dstIndices[iIdx++] = 0;
	dstIndices[iIdx++] = 1;
	dstIndices[iIdx++] = 2;
	dstIndices[iIdx++] = 2;
	dstIndices[iIdx++] = 3;
	dstIndices[iIdx++] = 0;
	
	indexType = IndexType::Index16Bit;
	numVertices = uint32(NumPlaneVerts);
	numIndices = uint32(NumPlaneIndices);

	meshParts.Init(1);
	MeshPart& part = meshParts[0];
	part.IndexStart = 0;
	part.IndexCount = numIndices;
	part.VertexStart = 0;
	part.VertexCount = numVertices;
	part.MaterialIdx = materialIdx;
}

#define NumSphereVerts(uDivisions, vDivisions) ((vDivisions - 1) * (uDivisions + 1) + 2)
#define NumSphereIndices(uDivisions, vDivisions) (6 * uDivisions * (vDivisions - 1))

void Mesh::InitSphere(const Float3& position, const float scale, const Quaternion& orientation, uint32 materialIdx,
	MeshVertex* sphereVerts, uint16* sphereIndices, uint64 uDivisions, uint64 vDivisions)
{
	Assert_(uDivisions >= 3);
	Assert_(vDivisions >= 3);

	const uint64 numSphereVerts = NumSphereVerts(uDivisions, vDivisions);
	Assert_(numSphereVerts <= UINT16_MAX);

	const uint64 numSphereIndices = NumSphereIndices(uDivisions, vDivisions);
	Assert_(numSphereIndices <= UINT16_MAX);

	uint64 vIdx = 0;

	// Add the vert at the top
	sphereVerts[vIdx++] = MeshVertex(Float3(0.0f, 1.0f, 0.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f));

	// Add the rings
	for (uint64 v = 1; v < vDivisions; ++v)
	{
		for (uint64 u = 0; u <= uDivisions; ++u)
		{
			const float theta = (float(v) / vDivisions) * Pi;
			const float phi = (float(u) / uDivisions) * Pi2;

			Float3 pos;
			pos.x = std::sin(theta) * std::cos(phi);
			pos.y = std::cos(theta);
			pos.z = std::sin(theta) * std::sin(phi);

			Float3 tangent;
			tangent.x = -std::sin(theta) * std::sin(phi);
			tangent.y = 0;
			tangent.z = std::sin(theta) * std::cos(phi);

			Float3 bitangent;
			bitangent.x = std::cos(theta) * std::cos(phi);
			bitangent.y = -std::sin(theta);
			bitangent.z = std::cos(theta) * std::sin(phi);

			Float2 uv;
			uv.x = phi / Pi2;
			uv.y = theta / Pi;
			
			sphereVerts[vIdx++] = MeshVertex(pos, Float3::Normalize(pos), uv, tangent, bitangent);
		}
	}

	// Add the vert at the bottom
	const uint64 lastVertIdx = vIdx;
	sphereVerts[vIdx++] = MeshVertex(Float3(0.0f, -1.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
	Assert_(vIdx == numSphereVerts);

	for (uint64 i = 0; i < numSphereVerts; ++i)
		sphereVerts[i].Transform(position, Float3(scale) * 0.5f, orientation);

	uint64 iIdx = 0;
	// Add the top ring of triangles
	for (uint64 u = 0; u < uDivisions; ++u)
	{
		sphereIndices[iIdx++] = uint16(0);
		sphereIndices[iIdx++] = uint16(u + 2);
		sphereIndices[iIdx++] = uint16(u + 1);
	}

	// Add the rest of the rings
	uint64 prevRowStart = 1;
	uint64 currRowStart = uDivisions + 2;
	for (uint64 v = 1; v < vDivisions - 1; ++v)
	{
		for (uint64 u = 0; u < uDivisions; ++u)
		{
			uint64 nextBottom = currRowStart + u + 1;
			uint64 nextTop = prevRowStart + u + 1;

			sphereIndices[iIdx++] = uint16(prevRowStart + u);
			sphereIndices[iIdx++] = uint16(nextTop);
			sphereIndices[iIdx++] = uint16(currRowStart + u);

			sphereIndices[iIdx++] = uint16(currRowStart + u);
			sphereIndices[iIdx++] = uint16(nextTop);
			sphereIndices[iIdx++] = uint16(nextBottom);
		}

		prevRowStart = currRowStart;
		currRowStart += uDivisions + 1;
	}

	// Add the last ring at the bottom
	const uint64 lastRingStart = uint64(lastVertIdx - uDivisions - 1);
	for (uint64 u = 0; u < uDivisions; ++u)
	{	
		sphereIndices[iIdx++] = uint16(lastVertIdx);
		sphereIndices[iIdx++] = uint16(lastRingStart + u);
		sphereIndices[iIdx++] = uint16(lastRingStart + u + 1);		
	}
	Assert_(iIdx == numSphereIndices);
	
	indexType = IndexType::Index16Bit;
	numVertices = uint32(numSphereVerts);
	numIndices = uint32(numSphereIndices);

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

void Model::GenerateBox(const Float3& position, const Float3& dimensions, const Quaternion& orientation,
	const wchar* colorMap, const wchar* normalMap, const wchar* roughnessMap)
{
	meshMaterials.Init(1);
	MeshMaterial& material = meshMaterials[0];
	material.TextureNames[uint64(MaterialTextureType::Albedo)] = colorMap;
	material.TextureNames[uint64(MaterialTextureType::Normal)] = normalMap;
	material.TextureNames[uint64(MaterialTextureType::Roughness)] = roughnessMap;

	wstring fileDirectory = L"Content\\Textures\\";
	LoadMaterialResources(meshMaterials, fileDirectory, false, materialTextures);

	vertices.Init(NumBoxVerts);
	indices.Init(NumBoxIndices);

	meshes.Init(1);
	meshes[0].InitBox(position, dimensions, orientation, 0, vertices.Data(), indices.Data());

	CreateBuffers();
}

void Model::GenerateTestScene()
{
	meshMaterials.Init(2);
	meshMaterials[0].TextureNames[uint64(MaterialTextureType::Albedo)] = L"Default.dds";
	meshMaterials[0].TextureNames[uint64(MaterialTextureType::Normal)] = L"DefaultNormalMap.dds";
	
	meshMaterials[1].TextureNames[uint64(MaterialTextureType::Albedo)] = L"Sponza_Bricks_a_Albedo.png";
	meshMaterials[1].TextureNames[uint64(MaterialTextureType::Normal)] = L"Sponza_Bricks_a_Normal.png";
	meshMaterials[1].TextureNames[uint64(MaterialTextureType::Roughness)] = L"Sponza_Bricks_a_Roughness.png";

	wstring fileDirectory = L"Content\\Textures\\";
	LoadMaterialResources(meshMaterials, fileDirectory, false, materialTextures);

	const uint64 uDivisions = 20;
	const uint64 vDivisions = 20;
	const uint64 numVerts = NumPlaneVerts + NumBoxIndices + NumSphereVerts(uDivisions, vDivisions);
	const uint64 numIndices = NumPlaneIndices + NumBoxIndices + NumSphereIndices(uDivisions, vDivisions);

	vertices.Init(numVerts);
	indices.Init(numIndices);

	meshes.Init(3);
	meshes[0].InitPlane(Float3(0.0f, 0.0f, 0.0f), Float2(10.0f), Quaternion(), 0, vertices.Data(), indices.Data());
	meshes[1].InitBox(Float3(0.0f, 1.0f, 0.0f), Float3(2.0f), Quaternion::FromEuler(0, Framework::DegToRad(45), 0), 1, &vertices[NumPlaneVerts], &indices[NumPlaneIndices]);
	meshes[2].InitSphere(Float3(0.0f, 2.5f, 0.0f), 1, Quaternion(), 0, &vertices[NumPlaneVerts + NumBoxVerts],
		&indices[NumPlaneIndices + NumBoxIndices], uDivisions, vDivisions);

	/*const uint64 uDivisions = 20;
	const uint64 vDivisions = 20;

	vertices.Init(NumSphereVerts(uDivisions, vDivisions));
	indices.Init(NumSphereIndices(uDivisions, vDivisions));

	meshes.Init(1);
	meshes[0].InitSphere(Float3(0.0f, 2.5f, 0.0f), 1, Quaternion(), 0, vertices.Data(),	indices.Data(), uDivisions, vDivisions);*/

	CreateBuffers();
}

void Model::Shutdown()
{
	for (uint64 i = 0; i < meshes.Size(); ++i)
		meshes[i].Shutdown();
	meshes.Shutdown();
	meshMaterials.Shutdown();
	for (uint64 i = 0; i < materialTextures.Count(); ++i)
	{
		materialTextures[i]->Texture.Shutdown();
		delete materialTextures[i];
		materialTextures[i] = nullptr;
	}
	materialTextures.Shutdown();
	forceSRGB = false;

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