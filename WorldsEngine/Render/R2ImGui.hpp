#pragma once
#include <ImGui/imgui.h>

namespace R2::VK
{
    class Core;
    class CommandBuffer;
}

namespace R2
{
    class BindlessTextureManager;
}

bool ImGui_ImplR2_Init(R2::VK::Core *core, R2::BindlessTextureManager *texMan);
void ImGui_ImplR2_Shutdown();
void ImGui_ImplR2_NewFrame();
void ImGui_ImplR2_RenderDrawData(ImDrawData *drawData, R2::VK::CommandBuffer &cb);