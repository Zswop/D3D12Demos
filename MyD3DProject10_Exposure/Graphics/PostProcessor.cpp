#include "PostProcessor.h"
#include "DX12.h"

namespace Framework
{

static const uint32 ReductionTGSize = 16;

enum ReductionRootParams : uint32
{
	ReductionParams_StandardDescriptors = 0,
	ReductionParams_UAV,
	ReductionParams_CBuffer,

	NumReductionRootParams
};

void PostProcessor::Initialize()
{
	helper.Initialize();

	// Load the shaders
	uberPost = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "UberPost", ShaderType::Pixel);
	scale = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "Scale", ShaderType::Pixel);
	bloom = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "Bloom", ShaderType::Pixel);
	blurH = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "BlurH", ShaderType::Pixel);
	blurV = CompileFromFile(L"Shaders\\PostProcessing.hlsl", "BlurV", ShaderType::Pixel);
	
	CompileOptions opts;
	const std::wstring shaderPath = L"Shaders\\LuminanceReductionCS.hlsl";
	reductionInitialCS = CompileFromFile(shaderPath.c_str(), "LuminanceReductionInitialCS", ShaderType::Compute, opts);	
	reductionCS = CompileFromFile(shaderPath.c_str(), "LuminanceReductionCS", ShaderType::Compute, opts);

	opts.Add("FinalPass_", 1);
	reductionFinalCS = CompileFromFile(shaderPath.c_str(), "LuminanceReductionCS", ShaderType::Compute, opts);

	{
		D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
		uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRanges[0].NumDescriptors = 1;
		uavRanges[0].BaseShaderRegister = 0;
		uavRanges[0].RegisterSpace = 0;
		uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameters[NumReductionRootParams] = {};
		rootParameters[ReductionParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ReductionParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[ReductionParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
		rootParameters[ReductionParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

		rootParameters[ReductionParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[ReductionParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[ReductionParams_CBuffer].Constants.Num32BitValues = 1;
		rootParameters[ReductionParams_CBuffer].Constants.RegisterSpace = 0;
		rootParameters[ReductionParams_CBuffer].Constants.ShaderRegister = 0;

		rootParameters[ReductionParams_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ReductionParams_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[ReductionParams_UAV].DescriptorTable.pDescriptorRanges = uavRanges;
		rootParameters[ReductionParams_UAV].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Point, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
		rootSignatureDesc.pStaticSamplers = staticSamplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		DX12::CreateRootSignature(&reductionRootSignature, rootSignatureDesc);
	}
}

void PostProcessor::Shutdown()
{
	helper.Shutdown();
	DX12::Release(reductionRootSignature);
	for (int i = 0; i < reductionTargets.Size(); ++i)
		reductionTargets[i].Shutdown();
	reductionTargets.Shutdown();
}

void PostProcessor::CreatePSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.CS = reductionInitialCS.ByteCode();
	psoDesc.pRootSignature = reductionRootSignature;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	DX12::Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&reductionInitialPSO));

	psoDesc.CS = reductionCS.ByteCode();
	DX12::Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&reductionPSO));

	psoDesc.CS = reductionFinalCS.ByteCode();
	DX12::Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&reductionFinalPSO));
}

void PostProcessor::DestroyPSOs()
{
	DX12::DeferredRelease(reductionInitialPSO);
	DX12::DeferredRelease(reductionFinalPSO);
	DX12::DeferredRelease(reductionPSO);
	helper.ClearCache();
}

void PostProcessor::AfterReset(uint32 width, uint32 height)
{
	uint64 numsReduction = 0;
	uint64 size = std::max(width, height);
	while (size > 1)
	{
		numsReduction++;
		size = DX12::DispatchSize(size, ReductionTGSize);
	}
	
	for (uint64 i = 0; i < reductionTargets.Size(); ++i)
		reductionTargets[i].Shutdown();
	reductionTargets.Init(numsReduction);

	uint64 w = width;
	uint64 h = height;

	for (uint64 i = 0; i < numsReduction; ++i)
	{
		w = DX12::DispatchSize(w, ReductionTGSize);
		h = DX12::DispatchSize(h, ReductionTGSize);

		RenderTextureInit rtInit;
		rtInit.Width = w;
		rtInit.Height = h;
		rtInit.Format = DXGI_FORMAT_R32_FLOAT;
		rtInit.CreateUAV = true;
		rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtInit.Name = L"Reduction Target";

		RenderTexture& rt = reductionTargets[i];
		rt.Initialize(rtInit);
	}

	adaptedLuminanceSRV = reductionTargets[reductionTargets.Size()-1].SRV();
}

void PostProcessor::Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output)
{
	PIXMarker marker(cmdList, "PostProcessing");

	helper.Begin(cmdList);

	CalcAvgLuminance(cmdList, input);

	TempRenderTarget* bloomTarget = Bloom(cmdList, input);

	// Apply tone mapping
	uint32 inputs[3] = { input.SRV(), bloomTarget->RT.SRV(), adaptedLuminanceSRV };
	const RenderTexture* outputs[1] = { &output };
	helper.PostProcess(uberPost, "Tone Mapping", inputs, ArraySize_(inputs), outputs, ArraySize_(outputs));
	
	bloomTarget->InUse = false;

	helper.End();
}

TempRenderTarget* PostProcessor::Bloom(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input)
{
	PIXMarker marker(cmdList, "Bloom");

	const uint64 bloomWidth = input.Texture.Width / 2;
	const uint64 bloomHeight = input.Texture.Height / 2;

	TempRenderTarget* downscale1 = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
	downscale1->RT.MakeWritable(cmdList);

	helper.PostProcess(bloom, "Bloom Initial Pass", input, downscale1);

	TempRenderTarget* blurTemp = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
	downscale1->RT.MakeReadable(cmdList);

	for (uint64 i = 0; i < 2; ++i)
	{
		blurTemp->RT.MakeWritable(cmdList);

		helper.PostProcess(blurH, "Horizontal Bloom Blur", downscale1, blurTemp);

		blurTemp->RT.MakeReadable(cmdList);
		downscale1->RT.MakeWritable(cmdList);

		helper.PostProcess(blurV, "Vertical Bloom Blur", blurTemp, downscale1);
		downscale1->RT.MakeReadable(cmdList);
	}

	blurTemp->InUse = false;

	return downscale1;
}

void PostProcessor::CalcAvgLuminance(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input)
{
	PIXMarker marker(cmdList, "Calc AvgLuminance");
		
	cmdList->SetPipelineState(reductionInitialPSO);
	cmdList->SetComputeRootSignature(reductionRootSignature);

	DX12::BindStandardDescriptorTable(cmdList, ReductionParams_StandardDescriptors, CmdListMode::Compute);

	D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { reductionTargets[0].UAV };
	DX12::BindTempDescriptorTable(cmdList, uavs, ArraySize_(uavs), ReductionParams_UAV, CmdListMode::Compute);

	cmdList->SetComputeRoot32BitConstant(ReductionParams_CBuffer, input.SRV(), 0);

	reductionTargets[0].MakeWritableUAV(cmdList);

	uint32 dispatchX = (uint32)reductionTargets[0].Width();
	uint32 dispatchY = (uint32)reductionTargets[0].Height();
	cmdList->Dispatch(dispatchX, dispatchY, 1);
	
	for (uint64 i = 1; i < reductionTargets.Size(); ++i)
	{
		if (i == (reductionTargets.Size() - 1))
			cmdList->SetPipelineState(reductionFinalPSO);
		else
			cmdList->SetPipelineState(reductionPSO);

		cmdList->SetComputeRootSignature(reductionRootSignature);

		DX12::BindStandardDescriptorTable(cmdList, ReductionParams_StandardDescriptors, CmdListMode::Compute);

		D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { reductionTargets[i].UAV };
		DX12::BindTempDescriptorTable(cmdList, uavs, ArraySize_(uavs), ReductionParams_UAV, CmdListMode::Compute);

		cmdList->SetComputeRoot32BitConstant(ReductionParams_CBuffer, reductionTargets[i-1].SRV(), 0);
		
		reductionTargets[i-1].MakeReadableUAV(cmdList);
		reductionTargets[i].MakeWritableUAV(cmdList);

		dispatchX = (uint32)reductionTargets[i].Width();
		dispatchY = (uint32)reductionTargets[i].Height();
		cmdList->Dispatch(dispatchX, dispatchY, 1);
	}

	reductionTargets[reductionTargets.Size()-1].MakeReadableUAV(cmdList);
}

}