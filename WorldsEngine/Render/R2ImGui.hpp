#pragma once
#include <ImGui/imgui.h>

namespace R2::VK
{
    class Core;
    class CommandBuffer;
}

bool ImGui_ImplR2_Init(R2::VK::Core* core);
void ImGui_ImplR2_Shutdown();
void ImGui_ImplR2_NewFrame();
void ImGui_ImplR2_RenderDrawData(ImDrawData* drawData, R2::VK::CommandBuffer& cb);