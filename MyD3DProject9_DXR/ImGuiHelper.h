#pragma once

#include "PCH.h"
#include "F12_Math.h"
#include "ImGui\\imgui.h"

namespace Framework
{

class Window;

namespace ImGuiHelper
{

void Initialize(Window& window);
void Shutdown();

void CreatePSOs(DXGI_FORMAT rtFormat);
void DestroyPSOs();

void BeginFrame(uint32 displayWidth, uint32 displayHeight, float timeDelta);
void EndFrame(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, uint32 displayWidth, uint32 displayHeight);

}// namespace ImGuiHelper

inline ImVec2 ToImVec2(Float2 v)
{
	return ImVec2(v.x, v.y);
}

inline Float2 ToFloat2(ImVec2 v)
{
	return Float2(v.x, v.y);
}

inline ImColor ToImColor(Float3 v)
{
	return ImColor(v.x, v.y, v.z);
}

inline ImColor ToImColor(Float4 v)
{
	return ImColor(v.x, v.y, v.z, v.w);
}

} // namespace Framework