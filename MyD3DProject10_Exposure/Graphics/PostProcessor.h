#pragma once

#include "..\\PCH.h"
#include "GraphicsTypes.h"
#include "PostProcessHelper.h"

namespace Framework
{

class PostProcessor
{

public:

	void Initialize();
	void Shutdown();

	void CreatePSOs();
	void DestroyPSOs();

	void AfterReset(uint32 width, uint32 height);
	void Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output);

protected:

	TempRenderTarget* Bloom(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input);
	void CalcAvgLuminance(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input);

	Array<RenderTexture> reductionTargets;
	uint32 adaptedLuminanceSRV;

	CompiledShaderPtr reductionInitialCS;
	CompiledShaderPtr reductionFinalCS;
	CompiledShaderPtr reductionCS;
	
	ID3D12RootSignature* reductionRootSignature = nullptr;
	ID3D12PipelineState* reductionInitialPSO = nullptr;
	ID3D12PipelineState* reductionFinalPSO = nullptr;
	ID3D12PipelineState* reductionPSO = nullptr;

	PostProcessHelper helper;

	PixelShaderPtr uberPost;
	PixelShaderPtr scale;
	PixelShaderPtr bloom;
	PixelShaderPtr blurH;
	PixelShaderPtr blurV;
};

}