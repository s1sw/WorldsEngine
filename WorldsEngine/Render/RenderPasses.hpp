#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include <glm/glm.hpp>

namespace worlds {
    struct MultiVP;
    struct LightUB;
    struct ModelMatrices;
    struct VulkanHandles;
    class RenderTexture;
    class Swapchain;
    class VKRenderer;
    class VRCullMeshRenderer {
    private:
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        vk::DescriptorSet ds;
        vku::GenericBuffer vertexBuf;
        vk::UniqueRenderPass renderPass;
        uint32_t totalVertCount;
        uint32_t leftVertCount;
    public:
        void setup(PassSetupCtx& ctx, vk::RenderPass& rp);
        void draw(vk::CommandBuffer& cmdBuf);
    };

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
        vku::UniformBuffer modelMatrixUB;
        vku::GenericBuffer pickingBuffer;
        vku::GenericBuffer lineVB;
        uint32_t currentLineVBSize;
        int numLineVerts;

        vk::ShaderModule fragmentShader;
        vk::ShaderModule vertexShader;

        vk::ShaderModule wireFragmentShader;
        vk::ShaderModule wireVertexShader;

        vk::UniqueSampler albedoSampler;
        vk::UniqueSampler shadowSampler;

        vk::UniqueFramebuffer renderFb;
        vk::UniqueDescriptorSet descriptorSet;

        RenderTexture* depthStencilImage;
        RenderTexture* polyImage;
        RenderTexture* shadowImage;

        bool enablePicking;
        int pickX, pickY;
        vk::UniqueEvent pickEvent;
        bool pickThisFrame;
        bool awaitingResults;
        bool setEventNextFrame;
        bool dsUpdateNeeded = false;

        void updateDescriptorSets(PassSetupCtx& ctx);
        VRCullMeshRenderer* cullMeshRenderer;
        uint32_t lastSky = 0;
    public:
        PolyRenderPass(RenderTexture* depthStencilImage, RenderTexture* polyImage, RenderTexture* shadowImage, bool enablePicking = false);
        void setPickCoords(int x, int y) { pickX = x; pickY = y; }
        void setup(PassSetupCtx& ctx);
        void prePass(PassSetupCtx& ctx, RenderCtx& rCtx);
        void execute(RenderCtx& ctx);
        void requestEntityPick();
        void reuploadDescriptors() { dsUpdateNeeded = true; }
        bool getPickedEnt(uint32_t* out);
        void lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev);
        virtual ~PolyRenderPass();
    };

    struct CascadeMatrices;
    class ShadowmapRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniqueDescriptorSet ds;
        RenderTexture* shadowImage;
        vk::UniqueFramebuffer shadowFb;
        vk::ShaderModule shadowVertexShader;
        vk::ShaderModule shadowFragmentShader;
        vku::UniformBuffer matrixBuffer;
        uint32_t shadowmapRes;
        CascadeMatrices* matricesMapped;

        void createRenderPass(VulkanHandles&);
        void createDescriptorSet(VulkanHandles&);
    public:
        ShadowmapRenderPass(RenderTexture* shadowImage);
        void setup(PassSetupCtx& ctx);
        void prePass(PassSetupCtx& ctx, RenderCtx& rCtx);
        void execute(RenderCtx& ctx);
        virtual ~ShadowmapRenderPass();
    };

    class TonemapRenderPass {
    private:
        vk::ShaderModule tonemapShader;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSet descriptorSet;
        vk::UniqueDescriptorSet rDescriptorSet;
        vk::UniqueSampler sampler;
        RenderTexture* finalPrePresent;
        RenderTexture* finalPrePresentR;
        RenderTexture* hdrImg;
        RenderTexture* gtaoImg;
    public:
        TonemapRenderPass(RenderTexture* hdrImg, RenderTexture* finalPrePresent, RenderTexture* gtaoImg);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx);
        void setRightFinalImage(PassSetupCtx& ctx, RenderTexture* right);
        virtual ~TonemapRenderPass();
    };

    class BlurRenderPass {
        vk::ShaderModule shader;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        std::vector<vk::UniqueDescriptorSet> dses;
        vk::UniqueSampler samp;
        RenderTexture* src;
        RenderTexture* tmpTarget;
        VKRenderer* renderer;
        uint32_t numLayers;
    public:
        BlurRenderPass(VKRenderer* renderer, RenderTexture* tex);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx);
        virtual ~BlurRenderPass();
    };

    class GTAORenderPass {
        vk::ShaderModule shader;
        vk::UniqueDescriptorSetLayout dsl;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSet descriptorSet;
        vk::UniqueSampler samp;

        BlurRenderPass* brp;
        RenderTexture* depth, *out;
        VKRenderer* renderer;
        int frameIdx;
    public:
        GTAORenderPass(VKRenderer* renderer, RenderTexture* depth, RenderTexture* out);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx);
        virtual ~GTAORenderPass();
    };

    class ImGuiRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniqueFramebuffer fb;
        RenderTexture* target;
        Swapchain& currSwapchain;
    public:
        vk::RenderPass& getRenderPass() { return *renderPass; }
        ImGuiRenderPass(Swapchain& swapchain);
        void setup(PassSetupCtx& ctx);
        void execute(RenderCtx& ctx, vk::Framebuffer& currFb);
        virtual ~ImGuiRenderPass();
    };
}
