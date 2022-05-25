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

	void Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output);

protected:

	TempRenderTarget* Bloom(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input);

	PostProcessHelper helper;

	PixelShaderPtr toneMap;
	PixelShaderPtr scale;
	PixelShaderPtr bloom;
	PixelShaderPtr blurH;
	PixelShaderPtr blurV;
};

}

