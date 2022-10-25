#include <Core/AssetDB.hpp>
#include <Core/ConVar.hpp>
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Core/MaterialManager.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VK.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKSyncPrims.hpp>
#include <R2/VKTimestampPool.hpp>
#include <Render/FakeLitPipeline.hpp>
#include <Render/ObjectPickPass.hpp>
#include <Render/ParticleDataManager.hpp>
#include <Render/ParticleSimulator.hpp>
#include <Render/R2ImGui.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/StandardPipeline/StandardPipeline.hpp>
#include <Render/StandardPipeline/RenderMaterialManager.hpp>
#include <SDL_vulkan.h>
#include <Tracy.hpp>
#include <VR/OpenXRInterface.hpp>

namespace R2::VK
{
    extern int allocatedDescriptorSets;
}

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
        : timeAccumulator(0.0)
        , interfaces(initInfo.interfaces)
    {
        ZoneScoped;
        bool enableValidation = true;

#ifdef NDEBUG
        enableValidation = false;
        enableValidation |= EngineArguments::hasArgument("validation-layers");
#else
        enableValidation = !EngineArguments::hasArgument("no-validation-layers");
#endif

        std::vector<const char*> instanceExts;

        for (const std::string& s : initInfo.additionalInstanceExtensions)
        {
            instanceExts.push_back(s.c_str());
        }
        instanceExts.push_back(nullptr);

        std::vector<const char*> deviceExts;

        for (const std::string& s : initInfo.additionalDeviceExtensions)
        {
            deviceExts.push_back(s.c_str());
        }
        deviceExts.push_back(nullptr);

        core = new VK::Core(new LogDebugOutputReceiver, enableValidation, instanceExts.data(), deviceExts.data());
        VK::SwapchainCreateInfo sci{};

        SDL_Vulkan_CreateSurface(initInfo.window, core->GetHandles()->Instance, &sci.surface);

        swapchain = new VK::Swapchain(core, sci);
        frameFence = new VK::Fence(core->GetHandles(), VK::FenceFlags::CreateSignaled);

        debugStats = RenderDebugStats{};

        bindlessTextureManager = new R2::BindlessTextureManager(core);
        textureManager = new VKTextureManager(core, bindlessTextureManager);
        uiTextureManager = new VKUITextureManager(textureManager);
        renderMeshManager = new RenderMeshManager(core);
        timestampPool = new R2::VK::TimestampPool(core->GetHandles(), 4);

        if (!ImGui_ImplR2_Init(core, bindlessTextureManager))
            return;

        R2::VK::GraphicsDeviceInfo deviceInfo = core->GetDeviceInfo();
        logMsg(WELogCategoryRender, "Device name: %s", deviceInfo.Name);
        timestampPeriod = deviceInfo.TimestampPeriod;

        ShaderCache::setDevice(core);

        if (initInfo.enableVR)
        {
            // 0 means the size is set automatically
            xrPresentManager = new XRPresentManager(this, interfaces, 0, 0);
        }

        shadowmapManager = new ShadowmapManager(this);
        objectPickPass = new ObjectPickPass(this);

        particleDataManager = new ParticleDataManager(this);
        particleSimulator = new ParticleSimulator(this);

        *success = true;
    }

    VKRenderer::~VKRenderer()
    {
        core->WaitIdle();
        RenderMaterialManager::Shutdown();
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
        xrPresentManager.Reset();
        shadowmapManager.Reset();
        objectPickPass.Reset();
        particleSimulator.Reset();
        particleDataManager.Reset();
        ImGui_ImplR2_Shutdown();

        delete core;
    }

    void VKRenderer::frame(entt::registry& registry, float deltaTime)
    {
        ZoneScoped;

        timeAccumulator += (double)deltaTime;

        // Reset debug stats
        debugStats.numDrawCalls = 0;
        debugStats.numRTTPasses = (int)rttPasses.size();
        debugStats.numActiveRTTPasses = 0;
        debugStats.numTriangles = 0;

        if (xrPresentManager)
        {
            bool needsCompositorWait = false;
            for (VKRTTPass *pass: rttPasses)
            {
                if (pass->settings.outputToXR)
                    needsCompositorWait = true;
            }

            if (needsCompositorWait)
            {
                xrPresentManager->waitFrame();
            }
        }

        frameFence->WaitFor();
        frameFence->Reset();
        VK::Texture* swapchainImage = swapchain->Acquire(frameFence);
        ImGui_ImplR2_NewFrame();

        currentDebugLines = swapDebugLineBuffer(currentDebugLineCount);

        int width;
        int height;

        swapchain->GetSize(width, height);

        core->BeginFrame();

        VK::CommandBuffer cb = core->GetFrameCommandBuffer();

        // Read back previous frame's timestamps for GPU time
        uint64_t lastTimestamps[2];

        if (timestampPool->GetTimestamps(core->GetFrameIndex() * 2, 2, lastTimestamps))
        {
            lastGPUTime = (float)(lastTimestamps[1] - lastTimestamps[0]) * timestampPeriod;
        }

        timestampPool->Reset(cb, core->GetFrameIndex() * 2, 2);
        timestampPool->WriteTimestamp(cb, core->GetFrameIndex() * 2);

        glm::mat4 shadowViewMatrix = interfaces.mainCamera->getViewMatrix();
        for (VKRTTPass* pass : rttPasses)
        {
            if (pass->active && pass->settings.enableShadows)
            {
                if (pass->settings.setViewsFromXR)
                {
                    // TODO
                    shadowViewMatrix = pass->cam->getViewMatrix();
                }
                else
                {
                    shadowViewMatrix = pass->cam->getViewMatrix();
                }
            }
        }
        shadowmapManager->AllocateShadowmaps(registry);
        shadowmapManager->RenderShadowmaps(cb, registry, shadowViewMatrix);

        particleSimulator->execute(registry, cb, deltaTime);

        bool xrRendered = false;
        for (VKRTTPass* pass : rttPasses)
        {
            if (!pass->active && !pass->hdrDataRequested)
                continue;
            debugStats.numActiveRTTPasses++;

            // I don't like having to do this in the renderer, but doing it elsewhere introduces unnecessary latency
            // and frame-pacing issues :(
            if (pass->settings.setViewsFromXR)
            {
                const UnscaledTransform& leftEye = interfaces.vrInterface->getEyeTransform(Eye::LeftEye);
                const UnscaledTransform& rightEye = interfaces.vrInterface->getEyeTransform(Eye::RightEye);
                pass->setView(
                        0, glm::inverse(leftEye.getMatrix()),
                        interfaces.vrInterface->getEyeProjectionMatrix(Eye::LeftEye));

                pass->setView(
                        1, glm::inverse(rightEye.getMatrix()),
                        interfaces.vrInterface->getEyeProjectionMatrix(Eye::RightEye));
            }

            cb.BeginDebugLabel("RTT Pass", 0.0f, 0.0f, 0.0f);
            pass->pipeline->draw(pass->settings.registryOverride ? *pass->settings.registryOverride : registry, cb);

            pass->getFinalTarget()->Acquire(
                cb,
                VK::ImageLayout::ShaderReadOnlyOptimal,
                VK::AccessFlags::ShaderRead,
                VK::PipelineStageFlags::FragmentShader);

            if (pass->hdrDataRequested)
            {
                pass->hdrDataRequested = false;
                pass->downloadHDROutput(cb);
            }

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

        objectPickPass->execute(cb, registry);

        swapchainImage->Acquire(
            cb, VK::ImageLayout::PresentSrc, VK::AccessFlags::MemoryRead, VK::PipelineStageFlags::AllCommands);

        timestampPool->WriteTimestamp(cb, core->GetFrameIndex() * 2 + 1);
        bindlessTextureManager->UpdateDescriptorsIfNecessary();

        if (this->xrPresentManager && xrRendered)
        {
            xrPresentManager->beginFrame();
        }
        core->EndFrame();

        swapchain->Present();

        if (this->xrPresentManager && xrRendered)
            xrPresentManager->endFrame();
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

    RenderDebugStats& VKRenderer::getDebugStats()
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

        IRenderPipeline* renderPipeline = new StandardPipeline(interfaces);

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

    void VKRenderer::requestPick(PickParams params)
    {
        objectPickPass->requestPick(params);
    }

    bool VKRenderer::getPickResult(uint32_t& entityId)
    {
        return objectPickPass->getResult(entityId);
    }

    void VKRenderer::reloadShaders()
    {
        ShaderCache::clear();

        for (VKRTTPass* pass : rttPasses)
        {
            delete pass->pipeline;
            pass->pipeline = new StandardPipeline(interfaces);
            pass->pipeline->setup(pass);
        }

        particleSimulator.Reset();
        particleSimulator = new ParticleSimulator(this);
    }

    static ConVar showRenderDebugMenus{ "r_showExtraDebug", "0" };
    void VKRenderer::drawDebugMenus()
    {
        if (!showRenderDebugMenus) return;

        textureManager->showDebugMenu();
        RenderMaterialManager::ShowDebugMenu();

        ImGui::Text("%i descriptor sets allocated", R2::VK::allocatedDescriptorSets);
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

    ShadowmapManager* VKRenderer::getShadowmapManager()
    {
        return shadowmapManager.Get();
    }

    ParticleDataManager* VKRenderer::getParticleDataManager()
    {
        return particleDataManager.Get();
    }

    const DebugLine* VKRenderer::getCurrentDebugLines(size_t* count)
    {
        *count = currentDebugLineCount;
        return currentDebugLines;
    }

    double VKRenderer::getTime()
    {
        return timeAccumulator;
    }
}