#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "GraphicsTypes.h"

namespace Framework
{

class SwapChain
{

public:
	SwapChain();
	~SwapChain();

	void Initialize(HWND ouputWindow);
	void Shutdown();
	void Reset();
	void BeginFrame();
	void EndFrame();

	IDXGISwapChain4* D3DSwapChain() const { return swapChain; }
	const RenderTexture& BackBuffer() const { return backBuffers[backBufferIdx]; }

	DXGI_FORMAT Format() const { return format; }
	uint32 Width() const { return width; }
	uint32 Height() const { return height; }
	bool FullScreen() const { return fullScreen; }
	bool VSYNEnabled() const { return vsync; }
	uint32 NumVSYNIntervals() const{ return numVSYNIntervals; }
	HANDLE WaitableObject() const { return waitableObject; }

	void SetFormat(DXGI_FORMAT format_) { format = format_; };
	void SetWidth(uint32 width_) { width = width_; }
	void SetHeight(uint32 height_) { height = height_; }
	void SetFullScreen(bool enabled) { fullScreen = enabled; }
	void SetVSYNEnabled(bool enabled) { vsync = enabled; }
	void SetNumVSYNCIntervals(uint32 intervals) { numVSYNIntervals = intervals; }

protected:
	void CheckForSuitableOuput();
	void AfterReset();
	void PrepareFullScreenSettings();

	static const uint64 NumBackBuffers = 2;

	IDXGISwapChain4* swapChain = nullptr;
	uint32 backBufferIdx = 0;
	RenderTexture backBuffers[NumBackBuffers];
	HANDLE waitableObject = INVALID_HANDLE_VALUE;

	IDXGIOutput* output = nullptr;

	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	uint32 width = 1920;
	uint32 height = 1080;
	bool fullScreen = false;
	bool vsync = true;
	DXGI_RATIONAL refreshRate = { };
	uint32 numVSYNIntervals = 1;
};
}