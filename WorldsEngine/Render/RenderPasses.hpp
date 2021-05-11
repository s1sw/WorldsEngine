#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include "Render.hpp"
#include <glm/glm.hpp>
#include <slib/StaticAllocList.hpp>

namespace worlds {
    struct MultiVP;
    struct LightUB;
    struct ModelMatrices;
    struct RenderContext;
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
        uint32_t totalVertCount;
        uint32_t leftVertCount;
        VulkanHandles* handles;
    public:
        VRCullMeshRenderer(VulkanHandles* handles);
        void setup(RenderContext& ctx, vk::RenderPass& rp);
        void draw(vk::CommandBuffer& cmdBuf);
    };

    struct SubmeshDrawInfo {
        uint32_t materialIdx;
        uint32_t matrixIdx;
        vk::Buffer vb;
        vk::Buffer ib;
        uint32_t indexCount;
        uint32_t indexOffset;
        uint32_t cubemapIdx;
        glm::vec3 cubemapExt;
        glm::vec3 cubemapPos;
        glm::vec4 texScaleOffset;
        entt::entity ent;
        vk::Pipeline pipeline;
        uint32_t drawMiscFlags;
        bool opaque;
    };

    class DebugLinesPass {
    private:
        vk::UniquePipeline linePipeline;
        vk::UniquePipelineLayout linePipelineLayout;
        vk::UniqueDescriptorSetLayout lineDsl;
        vk::UniqueDescriptorSet lineDs;
        vku::GenericBuffer lineVB;
        uint32_t currentLineVBSize;
        int numLineVerts;
        VulkanHandles* handles;
    public:
        DebugLinesPass(VulkanHandles* handles);
        void setup(RenderContext& ctx, vk::RenderPass renderPass);
        void prePass(RenderContext&);
        void execute(RenderContext&);
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

        vk::UniquePipeline skyboxPipeline;
        vk::UniquePipelineLayout skyboxPipelineLayout;
        vk::UniqueDescriptorSetLayout skyboxDsl;
        vk::UniqueDescriptorSet skyboxDs;

        MultiVP* vpMapped;
        LightUB* lightMapped;
        ModelMatrices* modelMatricesMapped;

        vku::UniformBuffer vpUB;
        vku::UniformBuffer lightsUB;
        vku::GenericBuffer modelMatrixUB;
        vku::GenericBuffer pickingBuffer;

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

        void updateDescriptorSets(RenderContext& ctx);
        VRCullMeshRenderer* cullMeshRenderer;
        DebugLinesPass* dbgLinesPass;
        uint32_t lastSky = 0;
        VulkanHandles* handles;
    public:
        PolyRenderPass(VulkanHandles* handles, RenderTexture* depthStencilImage, RenderTexture* polyImage, RenderTexture* shadowImage, bool enablePicking = false);
        void setPickCoords(int x, int y) { pickX = x; pickY = y; }
        void setup(RenderContext& ctx);
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx);
        void requestEntityPick();
        void reuploadDescriptors() { dsUpdateNeeded = true; }
        bool getPickedEnt(uint32_t* out);
        void lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev);
        virtual ~PolyRenderPass();
    };

    struct CascadeMatrices;
    class ShadowCascadePass {
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
        VulkanHandles* handles;

        void createRenderPass();
        void createDescriptorSet();
    public:
        ShadowCascadePass(VulkanHandles* handles, RenderTexture* shadowImage);
        void setup();
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx);
        virtual ~ShadowCascadePass();
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
        VulkanHandles* handles;
    public:
        TonemapRenderPass(VulkanHandles* handles, RenderTexture* hdrImg, RenderTexture* finalPrePresent);
        void setup(RenderContext& ctx);
        void execute(RenderContext& ctx);
        void setRightFinalImage(RenderTexture* right);
        virtual ~TonemapRenderPass();
    };

    class ImGuiRenderPass {
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniqueFramebuffer fb;
        RenderTexture* target;
        Swapchain& currSwapchain;
        VulkanHandles* handles;
    public:
        vk::RenderPass& getRenderPass() { return *renderPass; }
        ImGuiRenderPass(VulkanHandles* handles, Swapchain& swapchain);
        void setup();
        void execute(vk::CommandBuffer&,
                uint32_t width, uint32_t height, vk::Framebuffer& currFb);
        virtual ~ImGuiRenderPass();
    };
}
