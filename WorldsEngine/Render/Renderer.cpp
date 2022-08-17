#include <Core/AssetDB.hpp>
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Core/MaterialManager.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VK.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKSyncPrims.hpp>
#include <Render/FakeLitPipeline.hpp>
#include <Render/R2ImGui.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/StandardPipeline/StandardPipeline.hpp>
#include <Tracy.hpp>
#include <SDL_vulkan.h>

using namespace R2;

namespace worlds
{
    class LogDebugOutputReceiver : public VK::IDebugOutputReceiver
    {
    public:
        void DebugMessage(const char* msg) override
        {
            logErr("VK: %s", msg);
        }
    };

    VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    {
        ZoneScoped;
        bool enableValidation = true;

#ifdef NDEBUG
        enableValidation = false;
        enableValidation |= EngineArguments::hasArgument("validation-layers");
#else
        enableValidation = !EngineArguments::hasArgument("no-validation-layers");
#endif

        core = new VK::Core(new LogDebugOutputReceiver, enableValidation);
        VK::SwapchainCreateInfo sci{};

        SDL_Vulkan_CreateSurface(initInfo.window, core->GetHandles()->Instance, &sci.surface);

        swapchain = new VK::Swapchain(core, sci);
        frameFence = new VK::Fence(core->GetHandles(), VK::FenceFlags::CreateSignaled);

        debugStats = RenderDebugStats{};

        bindlessTextureManager = new R2::BindlessTextureManager(core);
        textureManager = new VKTextureManager(core, bindlessTextureManager);
        uiTextureManager = new VKUITextureManager(textureManager);
        renderMeshManager = new RenderMeshManager(core);

        if (!ImGui_ImplR2_Init(core, bindlessTextureManager))
            return;

        R2::VK::GraphicsDeviceInfo deviceInfo = core->GetDeviceInfo();
        logMsg(WELogCategoryRender, "Device name: %s", deviceInfo.Name);

        ShaderCache::setDevice(core);

        if (initInfo.enableVR)
        {
            // 0 means the size is set automatically
            xrPresentManager = new XRPresentManager(core, 0, 0);
        }

        *success = true;
    }

    VKRenderer::~VKRenderer()
    {
        delete frameFence;
        delete swapchain;

        for (VKRTTPass* pass : rttPasses)
        {
            delete pass;
        }

        delete renderMeshManager;
        delete uiTextureManager;
        delete textureManager;
        delete bindlessTextureManager;

        delete core;
    }

    void VKRenderer::frame(entt::registry& registry)
    {
        ZoneScoped;
        frameFence->WaitFor();
        frameFence->Reset();
        VK::Texture* swapchainImage = swapchain->Acquire(frameFence);
        ImGui_ImplR2_NewFrame();

        currentDebugLines = swapDebugLineBuffer(currentDebugLineCount);

        int width;
        int height;

        swapchain->GetSize(width, height);

        core->BeginFrame();
        bindlessTextureManager->UpdateDescriptorsIfNecessary();

        VK::CommandBuffer cb = core->GetFrameCommandBuffer();

        bool xrRendered = false;
        for (VKRTTPass* pass : rttPasses)
        {
            if (!pass->active)
                continue;

            cb.BeginDebugLabel("RTT Pass", 0.0f, 0.0f, 0.0f);
            pass->pipeline->draw(pass->settings.registryOverride ? *pass->settings.registryOverride : registry, cb);

            pass->getFinalTarget()->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead,
                                            VK::PipelineStageFlags::FragmentShader);

            if (pass->settings.outputToXR)
            {
                xrPresentManager->copyFromLayered(cb, pass->getFinalTarget());
                xrRendered = true;
            }

            cb.EndDebugLabel();
        }

        // Draw ImGui directly to the swapchain
        VK::RenderPass rp;
        rp.ColorAttachment(swapchainImage, VK::LoadOp::Clear, VK::StoreOp::Store);
        rp.ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(0.f, 0.f, 0.f, 1.0f));
        rp.RenderArea(width, height);

        cb.BeginDebugLabel("Dear ImGui", 0.0f, 0.0f, 0.0f);

        rp.Begin(cb);
        ImGui_ImplR2_RenderDrawData(imguiDrawData, cb);
        rp.End(core->GetFrameCommandBuffer());

        cb.EndDebugLabel();

        swapchainImage->Acquire(cb, VK::ImageLayout::PresentSrc, VK::AccessFlags::MemoryRead,
                                VK::PipelineStageFlags::AllCommands);

        core->EndFrame();

        if (this->xrPresentManager && xrRendered)
            xrPresentManager->submit(vrUsedPose);

        swapchain->Present();
    }

    float VKRenderer::getLastGPUTime() const
    {
        return lastGPUTime;
    }

    void VKRenderer::setVRUsedPose(glm::mat4 usedPose)
    {
        vrUsedPose = usedPose;
    }

    void VKRenderer::setVsync(bool vsync)
    {
        swapchain->SetVsync(vsync);
    }

    bool VKRenderer::getVsync() const
    {
        return swapchain->GetVsync();
    }

    const RenderDebugStats& VKRenderer::getDebugStats() const
    {
        return debugStats;
    }

    IUITextureManager* VKRenderer::getUITextureManager()
    {
        return uiTextureManager;
    }

    void VKRenderer::setImGuiDrawData(void* drawData)
    {
        imguiDrawData = (ImDrawData*)drawData;
    }

    RTTPass* VKRenderer::createRTTPass(RTTPassSettings& ci)
    {
        ZoneScoped;
        if (ci.msaaLevel == 0)
        {
            ci.msaaLevel = 1;
        }

        IRenderPipeline* renderPipeline = new StandardPipeline(this);

        VKRTTPass* pass = new VKRTTPass(this, ci, renderPipeline);
        renderPipeline->setup(pass);

        rttPasses.push_back(pass);
        return pass;
    }

    void VKRenderer::destroyRTTPass(RTTPass* pass)
    {
        pass->active = false;
        delete static_cast<VKRTTPass*>(pass);
        rttPasses.erase(std::remove(rttPasses.begin(), rttPasses.end(), pass), rttPasses.end());
    }

    void VKRenderer::reloadShaders()
    {
        ShaderCache::clear();

        for (VKRTTPass* pass : rttPasses)
        {
            delete pass->pipeline;
            pass->pipeline = new StandardPipeline(this);
            pass->pipeline->setup(pass);
        }
    }

    R2::VK::Core* VKRenderer::getCore()
    {
        return core;
    }

    RenderMeshManager* VKRenderer::getMeshManager()
    {
        return renderMeshManager;
    }

    R2::BindlessTextureManager* VKRenderer::getBindlessTextureManager()
    {
        return bindlessTextureManager;
    }

    VKTextureManager* VKRenderer::getTextureManager()
    {
        return textureManager;
    }

    const DebugLine* VKRenderer::getCurrentDebugLines(size_t* count)
    {
        *count = currentDebugLineCount;
        return currentDebugLines;
    }
}