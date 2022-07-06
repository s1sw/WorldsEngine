#include <Render/RenderInternal.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKSyncPrims.hpp>
#include <R2/VKTexture.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <SDL_vulkan.h>
#include <Render/R2ImGui.hpp>
#include <Core/Log.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/FakeLitPipeline.hpp>
#include <Core/AssetDB.hpp>

using namespace R2;

namespace worlds {
    class LogDebugOutputReceiver : public VK::IDebugOutputReceiver {
    public:
        void DebugMessage(const char* msg) override {
            logErr("VK: %s", msg);
        }
    };

    VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success) {
        core = new VK::Core(new LogDebugOutputReceiver);
        VK::SwapchainCreateInfo sci{};

        SDL_Vulkan_CreateSurface(initInfo.window, core->GetHandles()->Instance, &sci.surface);
        
        swapchain = new VK::Swapchain(core, sci);
        frameFence = new VK::Fence(core->GetHandles(), VK::FenceFlags::CreateSignaled);

        debugStats = RenderDebugStats{};

        textureManager = new R2::BindlessTextureManager(core);
        uiTextureManager = new VKUITextureManager(core, textureManager);
        renderMeshManager = new RenderMeshManager(core);

        if (!ImGui_ImplR2_Init(core, textureManager)) return;

        R2::VK::GraphicsDeviceInfo deviceInfo = core->GetDeviceInfo();
        logMsg(WELogCategoryRender, "Device name: %s", deviceInfo.Name);

        ShaderCache::setDevice(core);


        *success = true;
    }

    VKRenderer::~VKRenderer() {
        delete frameFence;
        delete swapchain;

        delete renderMeshManager;
        delete uiTextureManager;
        delete textureManager;

        delete core;
    }


    void VKRenderer::frame(entt::registry& registry) {
        frameFence->WaitFor();
        frameFence->Reset();
        VK::Texture* swapchainImage = swapchain->Acquire(frameFence);
        ImGui_ImplR2_NewFrame();

        int width;
        int height;

        swapchain->GetSize(width, height);

        core->BeginFrame();
        textureManager->UpdateDescriptorsIfNecessary();

        VK::CommandBuffer cb = core->GetFrameCommandBuffer();

        for (VKRTTPass* pass : rttPasses) {
            if (!pass->active) continue;
            cb.BeginDebugLabel("RTT Pass", 0.0f, 0.0f, 0.0f);
            pass->pipeline->draw(registry, cb);

            cb.EndDebugLabel();
        }

        VK::RenderPass rp;
        rp.ColorAttachment(swapchainImage, VK::LoadOp::Clear, VK::StoreOp::Store);
        rp.ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(0.f, 0.f, 0.f, 1.0f));
        rp.RenderArea(width, height);

        swapchainImage->WriteLayoutTransition(core->GetFrameCommandBuffer(), VK::ImageLayout::PresentSrc, VK::ImageLayout::AttachmentOptimal);

        rp.Begin(cb);

        cb.BeginDebugLabel("Dear ImGui", 0.0f, 0.0f, 0.0f);
        ImGui_ImplR2_RenderDrawData(imguiDrawData, cb);
        cb.EndDebugLabel();

        rp.End(core->GetFrameCommandBuffer());

        swapchainImage->WriteLayoutTransition(core->GetFrameCommandBuffer(), VK::ImageLayout::AttachmentOptimal, VK::ImageLayout::PresentSrc);

        core->EndFrame();
        swapchain->Present();
    }

    float VKRenderer::getLastGPUTime() const {
        return lastGPUTime;
    }

    void VKRenderer::setVRPredictAmount(float amt) {

    }

    void VKRenderer::setVsync(bool vsync) {
        swapchain->SetVsync(vsync);
    }

    bool VKRenderer::getVsync() const {
        return swapchain->GetVsync();
    }

    const RenderDebugStats& VKRenderer::getDebugStats() const {
        return debugStats;
    }

    IUITextureManager* VKRenderer::getUITextureManager() {
        return uiTextureManager;
    }

    void VKRenderer::setImGuiDrawData(void* drawData) {
        imguiDrawData = (ImDrawData*)drawData;
    }

    RTTPass* VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {   
        IRenderPipeline* renderPipeline = new FakeLitPipeline(this);

        VKRTTPass* pass = new VKRTTPass(this, ci, renderPipeline);
        renderPipeline->setup(pass);

        rttPasses.push_back(pass);
        return pass;
    }

    void VKRenderer::destroyRTTPass(RTTPass* pass) {
        delete static_cast<VKRTTPass*>(pass);
        rttPasses.erase(std::remove(rttPasses.begin(), rttPasses.end(), pass), rttPasses.end());
    }

    R2::VK::Core* VKRenderer::getCore() {
        return core;
    }

    RenderMeshManager* VKRenderer::getMeshManager() {
        return renderMeshManager;
    }

    R2::BindlessTextureManager* VKRenderer::getBindlessTextureManager() {
        return textureManager;
    }
}