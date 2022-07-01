#include <Render/RenderInternal.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKSyncPrims.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <SDL_vulkan.h>
#include <Render/R2ImGui.hpp>
#include <Core/Log.hpp>
#include <entt/entity/registry.hpp>
#include <Render/ShaderCache.hpp>
#include <Core/AssetDB.hpp>

using namespace R2;

namespace worlds {
    class LogDebugOutputReceiver : public VK::IDebugOutputReceiver {
    public:
        void DebugMessage(const char* msg) override {
            logErr("VK: %s", msg);
        }
    };

    VK::Pipeline* pipeline;
    VkPipelineLayout pipelineLayout;
    VK::DescriptorSetLayout* dsl;
    VK::DescriptorSet* ds;
    VK::Buffer* multiVP;
    VK::Buffer* modelMatrices;

    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
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

        VK::BufferCreateInfo vpBci{ VK::BufferUsage::Uniform, sizeof(MultiVP), true };
        multiVP = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{ VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true };
        modelMatrices = core->CreateBuffer(modelMatrixBci);

        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};
        dslb
            .Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment)
            .Binding(1, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment);
        
        dsl = dslb.Build();

        ds = core->CreateDescriptorSet(dsl);

        VK::DescriptorSetUpdater dsu{core->GetHandles(), ds};
        dsu
            .AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, multiVP)
            .AddBuffer(1, 0, VK::DescriptorType::StorageBuffer, modelMatrices)
            .Update();
        
        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb
            .PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(dsl);
        pipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        vb.Attributes.push_back(VK::VertexAttribute{0, VK::TextureFormat::R32G32B32_SFLOAT, 0});
        vb.Attributes.push_back(VK::VertexAttribute{1, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, normal)});

        VK::ShaderModule& stdVert = ShaderCache::getModule(AssetDB::pathToId("Shaders/standard.vert.spv"));
        VK::ShaderModule& stdFrag = ShaderCache::getModule(AssetDB::pathToId("Shaders/standard.frag.spv"));

        VK::PipelineBuilder pb{core->GetHandles()};
        pb
            .PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout)
            .ColorAttachmentFormat(VK::TextureFormat::R8G8B8A8_SRGB)
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, stdFrag);
        
        pipeline = pb.Build();

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


    void VKRenderer::frame(entt::registry& reg) {
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

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrices->Map();
        for (VKRTTPass* pass : rttPasses) {
            if (!pass->active) continue;
            cb.BeginDebugLabel("RTT Pass", 0.0f, 0.0f, 0.0f);

            VK::RenderPass r;
            r.ColorAttachment(pass->sdrTarget, VK::LoadOp::Clear, VK::StoreOp::Store);
            r.ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(1.f, 0.f, 1.f, 1.f));
            r.RenderArea(pass->width, pass->height);
            r.Begin(cb);
            
            MultiVP multiVPs{};
            multiVPs.views[0] = pass->cam->getViewMatrix();
            multiVPs.projections[0] = pass->cam->getProjectionMatrix((float)pass->width / pass->height);

            core->QueueBufferUpload(multiVP, &multiVPs, sizeof(multiVPs), 0);

            uint32_t modelMatrixIndex = 0;

            cb.BindPipeline(pipeline);
            cb.BindGraphicsDescriptorSet(pipelineLayout, ds->GetNativeHandle(), 0);
            cb.SetViewport(VK::Viewport::Simple(pass->width, pass->height));
            cb.SetScissor(VK::ScissorRect::Simple(pass->width, pass->height));

            reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
                RenderMeshInfo& rmi = renderMeshManager->loadOrGet(wo.mesh);

                StandardPushConstants spc{};
                spc.modelMatrixID = modelMatrixIndex;
                cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout);

                modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();

                cb.BindVertexBuffer(0, renderMeshManager->getVertexBuffer(), rmi.vertsOffset);

                VK::IndexType vkIdxType = rmi.indexType == IndexType::Uint32 ? VK::IndexType::Uint32 : VK::IndexType::Uint16;
                cb.BindIndexBuffer(renderMeshManager->getIndexBuffer(), rmi.indexOffset, vkIdxType);
                
                for (int i = 0; i < rmi.numSubmeshes; i++) {
                    RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                    cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
                }
            });
            r.End(cb);

            cb.EndDebugLabel();
        }
        modelMatrices->Unmap();

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
        VKRTTPass* pass = new VKRTTPass(this, ci);
        rttPasses.push_back(pass);
        return pass;
    }

    void VKRenderer::destroyRTTPass(RTTPass* pass) {
        delete static_cast<VKRTTPass*>(pass);
        rttPasses.erase(std::remove(rttPasses.begin(), rttPasses.end(), pass), rttPasses.end());
    }
}