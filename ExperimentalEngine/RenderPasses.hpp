#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include <glm/glm.hpp>

namespace worlds {
    class Swapchain;
    class PolyRenderPass : public RenderPass {
    private:

        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipeline noBackfaceCullPipeline;
        vk::UniquePipeline depthPrePipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;

        vk::UniquePipeline wireframePipeline;
        vk::UniquePipelineLayout wireframePipelineLayout;

        vk::UniquePipeline linePipeline;
        vk::UniquePipelineLayout linePipelineLayout;
        vk::UniqueDescriptorSetLayout lineDsl;
        vk::UniqueDescriptorSet lineDs;

        vk::UniquePipeline skyboxPipeline;
        vk::UniquePipelineLayout skyboxPipelineLayout;
        vk::UniqueDescriptorSetLayout skyboxDsl;
        vk::UniqueDescriptorSet skyboxDs;

        vk::UniquePipeline pickingBufCsPipeline;
        vk::UniquePipelineLayout pickingBufCsLayout;
        vk::UniqueDescriptorSetLayout pickingBufCsDsl;
        vk::UniqueDescriptorSet pickingBufCsDs;

        vku::UniformBuffer vpUB;
        vku::UniformBuffer lightsUB;
        vku::UniformBuffer materialUB;
        vku::UniformBuffer modelMatrixUB;
        vku::GenericBuffer pickingBuffer;
        vku::GenericBuffer lineVB;
        int currentLineVBSize;
        int numLineVerts;

        vku::ShaderModule fragmentShader;
        vku::ShaderModule vertexShader;

        vku::ShaderModule wireFragmentShader;
        vku::ShaderModule wireVertexShader;

        vk::UniqueSampler albedoSampler;
        vk::UniqueSampler shadowSampler;

        vk::UniqueFramebuffer renderFb;
        vk::UniqueDescriptorSet descriptorSet;

        RenderImageHandle depthStencilImage;
        RenderImageHandle polyImage;
        RenderImageHandle shadowImage;

        bool enablePicking;
        int pickX, pickY;
        uint32_t pickedEnt;
        vk::UniqueEvent pickEvent;
        bool pickThisFrame;
        bool awaitingResults;
        bool setEventNextFrame;

        void updateDescriptorSets(PassSetupCtx& ctx);
    public:
        PolyRenderPass(RenderImageHandle depthStencilImage, RenderImageHandle polyImage, RenderImageHandle shadowImage, bool enablePicking = false);
        void setPickCoords(int x, int y) { pickX = x; pickY = y; }
        RenderPassIO getIO() override;
        void setup(PassSetupCtx& ctx) override;
        void prePass(PassSetupCtx& ctx, RenderCtx& rCtx) override;
        void execute(RenderCtx& ctx) override;
        void requestEntityPick();
        bool getPickedEnt(uint32_t* out);
        void lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev);
        virtual ~PolyRenderPass();
    };

    class ShadowmapRenderPass : public RenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        RenderImageHandle shadowImage;
        vk::UniqueFramebuffer shadowFb;
        vku::ShaderModule shadowVertexShader;
        vku::ShaderModule shadowFragmentShader;
        uint32_t shadowmapRes;
    public:
        ShadowmapRenderPass(RenderImageHandle shadowImage);
        RenderPassIO getIO() override;
        void setup(PassSetupCtx& ctx) override;
        void execute(RenderCtx& ctx) override;
        virtual ~ShadowmapRenderPass() {}
    };

    class TonemapRenderPass : public RenderPass {
    private:
        vku::ShaderModule tonemapShader;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSet descriptorSet;
        vk::UniqueDescriptorSet rDescriptorSet;
        vk::UniqueSampler sampler;
        RenderImageHandle finalPrePresent;
        RenderImageHandle finalPrePresentR;
        RenderImageHandle hdrImg;
    public:
        TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle finalPrePresent);
        RenderPassIO getIO() override;
        void setup(PassSetupCtx& ctx) override;
        void execute(RenderCtx& ctx) override;
        void setRightFinalImage(PassSetupCtx& ctx, RenderImageHandle right);
        virtual ~TonemapRenderPass();
    };

    class ImGuiRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniqueFramebuffer fb;
        RenderImageHandle target;
        Swapchain& currSwapchain;
    public:
        vk::RenderPass& getRenderPass() { return *renderPass; }
        ImGuiRenderPass(Swapchain& swapchain);
        void handleResize(PassSetupCtx& ctx, RenderImageHandle newTarget);
        RenderPassIO getIO();
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx, vk::Framebuffer& currFb);
        virtual ~ImGuiRenderPass();
    };
}