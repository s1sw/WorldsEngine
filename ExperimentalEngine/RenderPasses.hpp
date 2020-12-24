#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include <glm/glm.hpp>

namespace worlds {
    struct MultiVP;
    struct LightUB;
    struct ModelMatrices;
    struct RenderTextureResource;
    class Swapchain;
    class PolyRenderPass {
    private:

        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipeline noBackfaceCullPipeline;
        vk::UniquePipeline depthPrePipeline;
        vk::UniquePipeline alphaTestPipeline;
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

        MultiVP* vpMapped;
        LightUB* lightMapped;
        ModelMatrices* modelMatricesMapped;

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

        RenderTextureResource* depthStencilImage;
        RenderTextureResource* polyImage;
        RenderTextureResource* shadowImage;

        bool enablePicking;
        int pickX, pickY;
        vk::UniqueEvent pickEvent;
        bool pickThisFrame;
        bool awaitingResults;
        bool setEventNextFrame;

        void updateDescriptorSets(PassSetupCtx& ctx);
    public:
        PolyRenderPass(RenderTextureResource* depthStencilImage, RenderTextureResource* polyImage, RenderTextureResource* shadowImage, bool enablePicking = false);
        void setPickCoords(int x, int y) { pickX = x; pickY = y; }
        void setup(PassSetupCtx& ctx);
        void prePass(PassSetupCtx& ctx, RenderCtx& rCtx);
        void execute(RenderCtx& ctx);
        void requestEntityPick();
        bool getPickedEnt(uint32_t* out);
        void lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev);
        virtual ~PolyRenderPass();
    };

    class ShadowmapRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        RenderTextureResource* shadowImage;
        vk::UniqueFramebuffer shadowFb;
        vku::ShaderModule shadowVertexShader;
        vku::ShaderModule shadowFragmentShader;
        uint32_t shadowmapRes;
    public:
        ShadowmapRenderPass(RenderTextureResource* shadowImage);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx);
        virtual ~ShadowmapRenderPass() {}
    };

    class TonemapRenderPass {
    private:
        vku::ShaderModule tonemapShader;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSet descriptorSet;
        vk::UniqueDescriptorSet rDescriptorSet;
        vk::UniqueSampler sampler;
        RenderTextureResource* finalPrePresent;
        RenderTextureResource* finalPrePresentR;
        RenderTextureResource* hdrImg;
    public:
        TonemapRenderPass(RenderTextureResource* hdrImg, RenderTextureResource* finalPrePresent);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx);
        void setRightFinalImage(PassSetupCtx& ctx, RenderTextureResource* right);
        virtual ~TonemapRenderPass();
    };

    class ImGuiRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniqueFramebuffer fb;
        RenderTextureResource* target;
        Swapchain& currSwapchain;
    public:
        vk::RenderPass& getRenderPass() { return *renderPass; }
        ImGuiRenderPass(Swapchain& swapchain);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx, vk::Framebuffer& currFb);
        virtual ~ImGuiRenderPass();
    };
}
