#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include "RenderInternal.hpp"
#include <glm/glm.hpp>
#include <slib/StaticAllocList.hpp>
#include "robin_hood.h"

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
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout dsl;
        VkDescriptorSet ds;
        vku::GenericBuffer vertexBuf;
        uint32_t totalVertCount;
        uint32_t leftVertCount;
        VulkanHandles* handles;
    public:
        VRCullMeshRenderer(VulkanHandles* handles);
        void setup(RenderContext& ctx, VkRenderPass& rp, VkDescriptorPool descriptorPool);
        void draw(VkCommandBuffer& cmdBuf);
        ~VRCullMeshRenderer();
    };

    struct SubmeshDrawInfo {
        uint32_t materialIdx;
        uint32_t matrixIdx;
        VkBuffer vb;
        VkBuffer ib;
        uint32_t indexCount;
        uint32_t indexOffset;
        uint32_t cubemapIdx;
        glm::vec3 cubemapExt;
        glm::vec3 cubemapPos;
        glm::vec4 texScaleOffset;
        entt::entity ent;
        VkPipeline pipeline;
        uint32_t drawMiscFlags;
        bool opaque;
        bool dontPrepass;
    };

    class DebugLinesPass {
    private:
        VkPipeline linePipeline;
        VkPipelineLayout linePipelineLayout;
        VkDescriptorSetLayout lineDsl;
        VkDescriptorSet lineDs;
        vku::GenericBuffer lineVB;
        uint32_t currentLineVBSize;
        int numLineVerts;
        VulkanHandles* handles;
    public:
        DebugLinesPass(VulkanHandles* handles);
        void setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool);
        void prePass(RenderContext&);
        void execute(RenderContext&);
        ~DebugLinesPass();
    };

    class SkyboxPass {
    private:
        VkPipeline skyboxPipeline;
        VkPipelineLayout skyboxPipelineLayout;
        VkDescriptorSetLayout skyboxDsl;
        VkDescriptorSet skyboxDs;
        VkSampler sampler;
        VkImageView lastSkyImageView = nullptr;
        VulkanHandles* handles;
        uint32_t lastSky = 0;
        void updateDescriptors(RenderContext& ctx, uint32_t loadedSkyId);
    public:
        SkyboxPass(VulkanHandles* handles);
        void setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool);
        void prePass(RenderContext&);
        void execute(RenderContext&);
    };

    class DepthPrepass {
    private:
        VkPipeline depthPrePipeline;
        VkPipeline alphaTestPipeline;
        VulkanHandles* handles;
        VkPipelineLayout layout;
    public:
        DepthPrepass(VulkanHandles* handles);
        // Takes in the standard pipeline layout as an additional parameter
        void setup(RenderContext& ctx, VkRenderPass renderPass, VkPipelineLayout pipelineLayout);
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx, slib::StaticAllocList<SubmeshDrawInfo>& drawInfo);
    };

    struct FontChar {
        uint32_t codepoint;

        uint16_t x;
        uint16_t y;

        uint16_t width;
        uint16_t height;

        int16_t originX;
        int16_t originY;

        uint16_t advance;
    };

    struct SDFFont {
        robin_hood::unordered_flat_map<uint32_t, FontChar> characters;
        float width;
        float height;
        vku::TextureImage2D atlas;
        uint32_t index;
    };

    class WorldSpaceUIPass {
    private:
        VkPipeline textPipeline;
        VkDescriptorSet descriptorSet;
        VkDescriptorSetLayout descriptorSetLayout;
        VkPipelineLayout pipelineLayout;
        VulkanHandles* handles;
        VkSampler sampler;
        vku::GenericBuffer vb;
        vku::GenericBuffer ib;

        robin_hood::unordered_flat_map<AssetID, SDFFont> fonts;

        uint32_t nextFontIdx = 0u;
        size_t bufferCapacity = 0;
        SDFFont& getFont(AssetID id);
        void updateBuffers(entt::registry&);
        void loadFont(AssetID font);
    public:
        WorldSpaceUIPass(VulkanHandles* handles);
        void setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool);
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx);
    };

    class PolyRenderPass {
    private:
        VkRenderPass renderPass;
        VkPipeline pipeline;
        VkPipeline noBackfaceCullPipeline;
        VkPipeline alphaTestPipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout dsl;

        VkPipeline wireframePipeline;
        VkPipelineLayout wireframePipelineLayout;

        LightUB* lightMapped;
        std::vector<ModelMatrices*> modelMatricesMapped;

        vku::GenericBuffer lightsUB;
        std::vector<vku::GenericBuffer> modelMatrixUB;
        vku::GenericBuffer pickingBuffer;

        VkShaderModule fragmentShader;
        VkShaderModule vertexShader;

        VkShaderModule wireFragmentShader;
        VkShaderModule wireVertexShader;

        VkSampler albedoSampler;
        VkSampler shadowSampler;

        VkFramebuffer renderFb;
        std::vector<VkDescriptorSet> descriptorSets;

        RenderTexture* depthStencilImage;
        RenderTexture* polyImage;

        bool enablePicking;
        int pickX, pickY;
        VkEvent pickEvent;
        bool pickThisFrame;
        bool awaitingResults;
        bool setEventNextFrame;
        bool dsUpdateNeeded = false;

        void updateDescriptorSets(RenderContext& ctx);
        VRCullMeshRenderer* cullMeshRenderer;
        DebugLinesPass* dbgLinesPass;
        SkyboxPass* skyboxPass;
        DepthPrepass* depthPrepass;
        WorldSpaceUIPass* uiPass;
        VulkanHandles* handles;
    public:
        PolyRenderPass(VulkanHandles* handles, RenderTexture* depthStencilImage, RenderTexture* polyImage, bool enablePicking = false);
        void setPickCoords(int x, int y) { pickX = x; pickY = y; }
        void setup(RenderContext& ctx, VkDescriptorPool descriptorPool);
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx);
        void requestEntityPick();
        void reuploadDescriptors() { dsUpdateNeeded = true; }
        bool getPickedEnt(uint32_t* out);
        virtual ~PolyRenderPass();
    };

    struct CascadeMatrices;
    class ShadowCascadePass {
    private:
        VkRenderPass renderPass;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout dsl;
        VkDescriptorSet ds;
        RenderTexture* shadowImage;
        VkFramebuffer shadowFb;
        VkShaderModule shadowVertexShader;
        VkShaderModule shadowFragmentShader;
        vku::UniformBuffer matrixBuffer;
        uint32_t shadowmapRes;
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

    class AdditionalShadowsPass {
    private:
        VkRenderPass renderPass;
        VkFramebuffer fb;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout dsl;
        VulkanHandles* handles;
        glm::mat4 shadowMatrices[4];
        bool renderIdx[4];
    public:
        AdditionalShadowsPass(VulkanHandles* handles);
        void setup();
        void prePass(RenderContext& ctx);
        void execute(RenderContext& ctx);
    };

    class TonemapRenderPass {
    private:
        VkShaderModule tonemapShader;
        VkDescriptorSetLayout dsl;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSet descriptorSet;
        VkDescriptorSet rDescriptorSet;
        VkSampler sampler;
        VkDescriptorPool dsPool;
        RenderTexture* finalPrePresent;
        RenderTexture* finalPrePresentR;
        RenderTexture* hdrImg;
        VulkanHandles* handles;
    public:
        TonemapRenderPass(VulkanHandles* handles, RenderTexture* hdrImg, RenderTexture* finalPrePresent);
        void setup(RenderContext& ctx, VkDescriptorPool descriptorPool);
        void execute(RenderContext& ctx);
        void setRightFinalImage(RenderTexture* right);
        virtual ~TonemapRenderPass();
    };

    class ImGuiRenderPass {
    private:
        VkRenderPass renderPass;
        VkFramebuffer fb;
        RenderTexture* target;
        Swapchain& currSwapchain;
        VulkanHandles* handles;
    public:
        VkRenderPass& getRenderPass() { return renderPass; }
        ImGuiRenderPass(VulkanHandles* handles, Swapchain& swapchain);
        void setup();
        void execute(VkCommandBuffer&,
                uint32_t width, uint32_t height, VkFramebuffer& currFb);
        virtual ~ImGuiRenderPass();
    };
}
