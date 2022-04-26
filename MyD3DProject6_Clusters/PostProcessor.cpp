#include "PostProcessor.h"

void PostProcessor::Initialize()
{
	helper.Initialize();

	// Load the shaders
	toneMap = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "ToneMap", ShaderType::Pixel);
	scale = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "Scale", ShaderType::Pixel);
}

void PostProcessor::Shutdown()
{
	helper.Shutdown();
}

void PostProcessor::CreatePSOs()
{
}

void PostProcessor::DestroyPSOs()
{
	helper.ClearCache();
}

void PostProcessor::Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output)
{
	PIXMarker marker(cmdList, "PostProcessing");

	helper.Begin(cmdList);

	// Apply tone mapping
	uint32 inputs[1] = { input.SRV() };
	const RenderTexture* outputs[1] = { &output };
	helper.PostProcess(toneMap, "Tone Mapping", inputs, ArraySize_(inputs), outputs, ArraySize_(outputs));
	
	helper.End();
}