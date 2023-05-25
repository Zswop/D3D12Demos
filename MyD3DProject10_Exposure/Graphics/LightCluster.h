#pragma once

#include  "..\\PCH.h"
#include "..\\Containers.h"
#include "ShaderCompilation.h"
#include "Camera.h"
#include "Model.h"

namespace Framework
{

static const uint64 NumZTiles = 16;
static const uint64 ClusterTileSize = 16;
static const uint64 SpotLightElementsPerCluster = 1;

class LightCluster
{

public:

	LightCluster();

	void Initialize(const Array<ModelSpotLight>* spotLights);
	void Shutdown();

	void CreateClusterBuffer(uint32 width, uint32 height);

	void CreatePSOs(DXGI_FORMAT rtFormat);
	void DestroyPSOs();

	void UpdateLights(const Camera& camera);
	void RenderClusters(ID3D12GraphicsCommandList* cmdList, const Camera& camera);
	void RenderClusterVisualizer(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const Float2 displaySize);

	const uint64 NumXTiles() const { return numXTiles; }
	const uint64 NumYTiles() const { return numYTiles; }
	
	const RawBuffer& SpotLightClusterBuffer() const { return spotLightClusterBuffer; }

protected:

	uint64 numXTiles = 0;
	uint64 numYTiles = 0;

	const Array<ModelSpotLight>* spotLights = nullptr;

	StructuredBuffer spotLightClusterVtxBuffer;
	FormattedBuffer spotLightClusterIdxBuffer;
	Array<Float3> coneVertices;

	StructuredBuffer spotLightBoundsBuffer;
	StructuredBuffer spotLightInstanceBuffer;
	RawBuffer spotLightClusterBuffer;
	uint64 numIntersectingSpotLights = 0;

	CompiledShaderPtr clusterVS;
	CompiledShaderPtr clusterFrontFacePS;
	CompiledShaderPtr clusterBackFacePS;
	CompiledShaderPtr clusterIntersectingPS;
	ID3D12RootSignature* clusterRS = nullptr;
	ID3D12PipelineState* clusterFrontFacePSO = nullptr;
	ID3D12PipelineState* clusterBackFacePSO = nullptr;
	ID3D12PipelineState* clusterIntersectingPSO = nullptr;

	CompiledShaderPtr fullScreenTriVS;
	CompiledShaderPtr clusterVisPS;
	ID3D12RootSignature* clusterVisRS = nullptr;
	ID3D12PipelineState* clusterVisPSO = nullptr;
};

}
