#include <Render/RenderInternal.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKSyncPrims.hpp>
#include <SDL_vulkan.h>
#include <Render/R2ImGui.hpp>

using namespace R2;

namespace worlds {
    VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success) {
        core = new VK::Core;
        VK::SwapchainCreateInfo sci{};

        SDL_Vulkan_CreateSurface(initInfo.window, core->GetHandles()->Instance, &sci.surface);
        
        swapchain = new VK::Swapchain(core, sci);

        if (!ImGui_ImplR2_Init(core)) return;

        frameFence = new VK::Fence(core->GetHandles(), VK::FenceFlags::CreateSignaled);

        *success = true;
    }

    VKRenderer::~VKRenderer() {
        delete frameFence;
        delete swapchain;

        delete core;
    }

    void VKRenderer::frame() {
        frameFence->WaitFor();
        frameFence->Reset();
        VK::Texture* swapchainImage = swapchain->Acquire(frameFence);
        ImGui_ImplR2_NewFrame();

        int width;
        int height;

        swapchain->GetSize(width, height);

        core->BeginFrame();

        VK::RenderPass rp;
        rp.ColorAttachment(swapchainImage, VK::LoadOp::Clear, VK::StoreOp::Store);
        rp.ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(0.f, 0.f, 0.f, 1.0f));
        rp.RenderArea(width, height);

        VK::CommandBuffer cb = core->GetFrameCommandBuffer();

        rp.Begin(cb);

        ImGui_ImplR2_RenderDrawData(imguiDrawData, cb);

        rp.End(core->GetFrameCommandBuffer());

        core->EndFrame();
        swapchain->Present();
    }

    float VKRenderer::getLastGPUTime() const {
        return 1337.0f;
    }

    void VKRenderer::setVRPredictAmount(float amt) {

    }

    void VKRenderer::setVsync(bool vsync) {
        swapchain->SetVsync(vsync);
    }

    bool VKRenderer::getVsync() const {
        return true;
    }

    const RenderDebugStats& VKRenderer::getDebugStats() const {
        return debugStats;
    }

    void VKRenderer::setImGuiDrawData(void* drawData) {
        imguiDrawData = (ImDrawData*)drawData;
    }

    RTTPass* VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {   
        return new VKRTTPass(this);
    }

    void VKRenderer::destroyRTTPass(RTTPass* pass) {
        delete static_cast<VKRTTPass*>(pass);
    }
}