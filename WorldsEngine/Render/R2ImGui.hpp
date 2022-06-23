#pragma once
#include "../imgui.h"

namespace R2::VK
{
    class Renderer;
    class CommandBuffer;
}

bool ImGui_ImplR2_Init(R2::VK::Renderer* renderer);
void ImGui_ImplR2_Shutdown();
void ImGui_ImplR2_NewFrame();
void ImGui_ImplR2_RenderDrawData(ImDrawData* drawData, R2::VK::CommandBuffer& cb);