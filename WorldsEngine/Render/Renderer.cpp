#include <Render/RenderInternal.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSwapchain.hpp>
#include <SDL_vulkan.h>

namespace worlds {
    VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success) {
        core = new R2::VK::Core;
        R2::VK::SwapchainCreateInfo sci{};

        SDL_Vulkan_CreateSurface(initInfo.window, core->GetHandles()->Instance, &sci.surface);
        
        swapchain = new R2::VK::Swapchain(core, sci);
    }

    void VKRenderer::frame() {
        swapchain->Acquire();
        core->BeginFrame();
        core->EndFrame();
        swapchain->Present();
    }
}