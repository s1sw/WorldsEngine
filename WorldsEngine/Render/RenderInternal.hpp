#pragma once
#include "Render.hpp"
#include <Render/vku/vku.hpp>
#include <Render/ResourceSlots.hpp>
#include <tracy/TracyVulkan.hpp>

namespace worlds {
    class PolyRenderPass;
    class ImGuiRenderPass;
    class TonemapRenderPass;
    class ShadowCascadePass;
    class AdditionalShadowsPass;
    class GTAORenderPass;

    struct AOBox {
        glm::vec4 pack0, pack1, pack2, pack3;

        void setRotationMat(glm::mat3 mat) {
            pack0 = glm::vec4(mat[0][0], mat[0][1], mat[0][2], mat[1][0]);
            pack1 = glm::vec4(mat[1][1], mat[1][2], mat[2][0], mat[2][1]);
            pack2 = glm::vec4(mat[2][2], pack2.y, pack2.z, pack2.w);
        }

        void setTranslation(glm::vec3 t) {
            pack2 = glm::vec4(pack2.x, t.x, t.y, t.z);
        }

        void setMatrix(glm::mat4 m4) {
            pack0 = glm::vec4(m4[0][0], m4[0][1], m4[0][2], m4[1][0]);
            pack1 = glm::vec4(m4[1][1], m4[1][2], m4[2][0], m4[2][1]);
            pack2 = glm::vec4(m4[2][2], glm::vec3{ m4[3] });
        }

        void setScale(glm::vec3 s) {
            pack3 = glm::vec4(s, pack3.w);
        }

        void setEntityId(uint32_t id) {
            pack3.w = glm::uintBitsToFloat(id);
        }
    };

    struct AOSphere {
        glm::vec3 position;
        float radius;
    };

    struct LightUB {
        glm::mat4 additionalShadowMatrices[NUM_SHADOW_LIGHTS];
        glm::vec4 pack0;
        glm::vec4 pack1;
        glm::mat4 shadowmapMatrices[3];
        PackedLight lights[128];
        AOBox box[16];
        AOSphere sphere[16];
        uint32_t sphereIds[16];
    };

    struct QueueFamilyIndices {
        uint32_t graphics;
        uint32_t present;
    };

    struct ModelMatrices {
        static const uint32_t SIZE = 2048;
        glm::mat4 modelMatrices[2048];
    };

    struct MaterialsUB {
        PackedMaterial materials[256];
    };

    struct ShadowCascadeInfo {
        glm::mat4 matrices[3];
        float texelsPerUnit[3];
    };

    struct RenderDebugContext {
        RenderDebugStats* stats;
#ifdef TRACY_ENABLE
        std::vector<TracyVkCtx>* tracyContexts;
#endif
    };

    struct LoadedMeshData {
        vku::VertexBuffer vb;
        vku::IndexBuffer ib;
        uint32_t indexCount;
        vk::IndexType indexType;
        SubmeshInfo submeshes[NUM_SUBMESH_MATS];
        uint8_t numSubmeshes;
        float sphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    class Swapchain {
    public:
        Swapchain(vk::PhysicalDevice&, vk::Device&, vk::SurfaceKHR&, QueueFamilyIndices qfi, bool fullscreen, vk::SwapchainKHR oldSwapchain = vk::SwapchainKHR(), vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo);
        ~Swapchain();
        void getSize(uint32_t* x, uint32_t* y) { *x = width; *y = height; }
        vk::Result acquireImage(vk::Device& device, vk::Semaphore semaphore, uint32_t* imageIndex);
        vk::UniqueSwapchainKHR& getSwapchain() { return swapchain; }
        vk::Format imageFormat() { return format; }
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> imageViews;
    private:
        vk::Device& device;
        vk::UniqueSwapchainKHR swapchain;
        vk::Format format;
        uint32_t width;
        uint32_t height;
    };

    // Holds handles to useful Vulkan objects
    struct VulkanHandles {
        VKVendor vendor;
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        vk::PipelineCache pipelineCache;
        vk::DescriptorPool descriptorPool;
        vk::CommandPool commandPool;
        vk::Instance instance;
        VmaAllocator allocator;
        uint32_t graphicsQueueFamilyIdx;
        GraphicsSettings graphicsSettings;
        uint32_t width, height;
        uint32_t vrWidth, vrHeight;
    };

    struct RTResourceCreateInfo {
        vk::ImageCreateInfo ici;
        vk::ImageViewType viewType;
        vk::ImageAspectFlagBits aspectFlags;
    };

    class RenderTexture {
    public:
        vku::GenericImage image;
        vk::ImageAspectFlagBits aspectFlags;
    private:
        RenderTexture(VulkanHandles* ctx, RTResourceCreateInfo resourceCreateInfo, const char* debugName);
        friend class VKRenderer;
    };

    struct RenderResources {
        TextureSlots& textures;
        CubemapSlots& cubemaps;
        MaterialSlots& materials;
        std::unordered_map<AssetID, LoadedMeshData>& meshes;
        vku::GenericImage* brdfLut;
        vku::GenericBuffer* materialBuffer;
        vku::GenericBuffer* vpMatrixBuffer;
        RenderTexture* shadowCascades;
        RenderTexture** additionalShadowImages;
    };

    struct RenderContext {
        RenderResources resources;
        ShadowCascadeInfo cascadeInfo;
        RenderDebugContext debugContext;
        PassSettings passSettings;
        entt::registry& registry;

        glm::mat4 projMatrices[2];
        glm::mat4 viewMatrices[2];

        vk::CommandBuffer cmdBuf;
        uint32_t passWidth;
        uint32_t passHeight;
        uint32_t imageIndex;
        int maxSimultaneousFrames;
    };

    class BRDFLUTRenderer {
    public:
        BRDFLUTRenderer(VulkanHandles& ctx);
        void render(VulkanHandles& ctx, vku::GenericImage& target);
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;

        vku::ShaderModule vs;
        vku::ShaderModule fs;
    };

    class CubemapConvoluter {
    public:
        CubemapConvoluter(std::shared_ptr<VulkanHandles> ctx);
        void convolute(vku::TextureImageCube& cubemap);
    private:
        vku::ShaderModule cs;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        std::shared_ptr<VulkanHandles> vkCtx;
        vk::UniqueSampler sampler;
    };

    class PipelineCacheSerializer {
    public:
        static void loadPipelineCache(const vk::PhysicalDeviceProperties&, vk::PipelineCacheCreateInfo&);
        static void savePipelineCache(const vk::PhysicalDeviceProperties&, const vk::PipelineCache&, const vk::Device&);
    };

    class VKRenderer;

    class VKRTTPass : public RTTPass {
    public:
        void drawNow(entt::registry& world) override;
        RenderTexture* hdrTarget;
        RenderTexture* sdrFinalTarget;
        RenderTexture* depthTarget;
        void requestPick(int x, int y) override;
        bool getPickResult(uint32_t* result) override;
        float* getHDRData() override;
    private:
        VKRTTPass(const RTTPassCreateInfo& ci, VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats);
        ~VKRTTPass();
        PolyRenderPass* prp;
        TonemapRenderPass* trp;
        bool isVr;
        bool outputToScreen;
        bool enableShadows;
        Camera* cam;
        VKRenderer* renderer;
        IVRInterface* vrInterface;
        RenderDebugStats* dbgStats;
        void writeCmds(uint32_t frameIdx, vk::CommandBuffer buf, entt::registry& world);
        vk::UniqueDescriptorPool descriptorPool;

        friend class VKRenderer;
    };

    class VKRenderer : public Renderer {
        const static uint32_t NUM_TEX_SLOTS = 256;
        const static uint32_t NUM_MAT_SLOTS = 256;
        const static uint32_t NUM_CUBEMAP_SLOTS = 64;

        struct RTTPassInternal {
            PolyRenderPass* prp;
            TonemapRenderPass* trp;
            uint32_t width, height;
            RenderTexture* hdrTarget;
            RenderTexture* sdrFinalTarget;
            RenderTexture* depthTarget;
            bool isVr;
            bool outputToScreen;
            bool enableShadows;
            bool active;
            Camera* cam;
        };

        vk::UniqueInstance instance;
        vk::PhysicalDevice physicalDevice;
        vk::UniqueDevice device;
        vk::UniquePipelineCache pipelineCache;
        vk::UniqueDescriptorPool descriptorPool;
        vk::SurfaceKHR surface;
        std::unique_ptr<Swapchain> swapchain;
        vku::DebugCallback dbgCallback;
        uint32_t graphicsQueueFamilyIdx;
        uint32_t presentQueueFamilyIdx;
        uint32_t asyncComputeQueueFamilyIdx;
        uint32_t width, height;
        vk::SampleCountFlagBits msaaSamples;
        int32_t numMSAASamples;
        vk::UniqueRenderPass imguiRenderPass;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        vk::UniqueCommandPool commandPool;
        std::vector<vk::UniqueCommandBuffer> cmdBufs;
        int maxFramesInFlight = 2;
        std::vector<vk::Semaphore> cmdBufferSemaphores;
        std::vector<vk::Semaphore> imgAvailable;
        std::vector<vk::Fence> cmdBufFences;
        std::vector<vk::Fence> imgFences;
        VmaAllocator allocator;
        vku::GenericBuffer materialUB;
        vku::GenericBuffer vpBuffer;
        VulkanHandles handles;

        RenderTexture* finalPrePresent;

        RenderTexture* leftEye;
        RenderTexture* rightEye;

        RenderTexture* shadowmapImage;
        RenderTexture* shadowImages[NUM_SHADOW_LIGHTS];
        RenderTexture* imguiImage;

        std::vector<vk::DescriptorSet> descriptorSets;
        SDL_Window* window;
        vk::UniqueQueryPool queryPool;
        uint64_t lastRenderTimeTicks;
        float timestampPeriod;

        std::vector<VKRTTPass*> rttPasses;
        std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
        std::vector<TracyVkCtx> tracyContexts;
        std::unique_ptr<TextureSlots> texSlots;
        std::unique_ptr<MaterialSlots> matSlots;
        std::unique_ptr<CubemapSlots> cubemapSlots;

        uint32_t shadowmapRes;
        bool enableVR;
        ImGuiRenderPass* irp;
        uint32_t vrWidth, vrHeight;
        IVRInterface* vrInterface;
        VrApi vrApi;
        float vrPredictAmount;
        bool clearMaterialIndices;
        bool isMinimised;
        bool useVsync;
        vku::GenericImage brdfLut;
        std::shared_ptr<CubemapConvoluter> cubemapConvoluter;
        bool swapchainRecreated;
        bool enablePicking;
        RenderDebugStats dbgStats;
        uint32_t frameIdx, lastFrameIdx;
        ShadowCascadePass* shadowCascadePass;
        AdditionalShadowsPass* additionalShadowsPass;
        void* rdocApi;

        void createSwapchain(vk::SwapchainKHR oldSwapchain);
        void createFramebuffers();
        void createSCDependents();
        void presentNothing(uint32_t imageIndex);
        vku::ShaderModule loadShaderAsset(AssetID id);
        void createInstance(const RendererInitInfo& initInfo);
        void submitToOpenVR();
        glm::mat4 getCascadeMatrix(bool forVr, Camera cam, glm::vec3 lightdir, glm::mat4 frustumMatrix, float& texelsPerUnit);
        void calculateCascadeMatrices(bool forVr, entt::registry& world, Camera& cam, RenderContext& rCtx);
        void writeCmdBuf(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg);
        void reuploadMaterials();

        friend class VKRTTPass;
    public:
        double time;
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        void recreateSwapchain() override;
        void frame(Camera& cam, entt::registry& reg) override;
        void preloadMesh(AssetID id) override;
        void unloadUnusedMaterials(entt::registry& reg) override;
        void reloadContent(ReloadFlags flags) override;
        float getLastRenderTime() const override { return lastRenderTimeTicks * timestampPeriod; }
        void setVRPredictAmount(float amt) override { vrPredictAmount = amt; }
        void setVsync(bool vsync) override { if (useVsync != vsync) { useVsync = vsync; recreateSwapchain(); } }
        bool getVsync() const override { return useVsync; }
        VulkanHandles* getHandles() { return &handles; }
        const RenderDebugStats& getDebugStats() const override { return dbgStats; }
        void uploadSceneAssets(entt::registry& reg) override;
        RenderResources getResources();

        VKRTTPass* createRTTPass(RTTPassCreateInfo& ci) override;
        void destroyRTTPass(RTTPass* pass) override;

        RenderTexture* createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName = nullptr);

        void triggerRenderdocCapture();
        void startRdocCapture();
        void endRdocCapture();

        ~VKRenderer();
    };
}