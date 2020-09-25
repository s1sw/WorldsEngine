#pragma once
#include <glm/glm.hpp>
#include "IVRInterface.hpp"
#include "tracy/TracyVulkan.hpp"
#include "ResourceSlots.hpp"
#include "Camera.hpp"
#include "RenderGraph.hpp"
#include <SDL2/SDL.h>
#include "Console.hpp"
#define NUM_SUBMESH_MATS 32

namespace worlds {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 uv;
    };

    struct ProceduralObject {
        ProceduralObject() : uploaded(false), readyForUpload(false), visible(true), materialIdx(~0u) {}
        AssetID material;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        bool uploaded;
        bool readyForUpload;
        bool visible;
        vku::VertexBuffer vb;
        vku::IndexBuffer ib;
        uint32_t indexCount;
        vk::IndexType indexType;
        uint32_t materialIdx;
        std::string dbgName;
    };

    struct MVP {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct VP {
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct MultiVP {
        glm::mat4 views[8];
        glm::mat4 projections[8];
        glm::vec4 viewPos[8];
    };

    struct PackedLight {
        glm::vec4 pack0;
        glm::vec4 pack1;
        glm::vec4 pack2;
    };

    struct LightUB {
        glm::vec4 pack0;
        glm::mat4 shadowmapMatrix;
        PackedLight lights[16];
    };

    struct QueueFamilyIndices {
        uint32_t graphics;
        uint32_t present;
    };

    class Swapchain {
    public:
        Swapchain(vk::PhysicalDevice&, vk::Device&, vk::SurfaceKHR&, QueueFamilyIndices qfi, vk::SwapchainKHR oldSwapchain = vk::SwapchainKHR(), vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo);
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

    class PolyRenderPass;
    class ImGuiRenderPass;

    typedef uint32_t RenderImageHandle;

    struct TextureUsage {
        vk::ImageLayout layout;
        vk::PipelineStageFlagBits stageFlags;
        vk::AccessFlagBits accessFlags;
        RenderImageHandle handle;
    };

    struct ImageBarrier {
        RenderImageHandle handle;
        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;
        vk::ImageAspectFlagBits aspectMask;
        vk::AccessFlagBits srcMask;
        vk::AccessFlagBits dstMask;
        vk::PipelineStageFlagBits srcStage;
        vk::PipelineStageFlagBits dstStage;
    };

    struct ModelMatrices {
        glm::mat4 modelMatrices[1024];
    };

    struct MaterialsUB {
        PackedMaterial materials[256];
    };

    struct GraphicsSettings {
        GraphicsSettings() : msaaLevel(2), shadowmapRes(1024), enableVr(false) {}
        GraphicsSettings(int msaaLevel, int shadowmapRes, bool enableVr)
            : msaaLevel(msaaLevel)
            , shadowmapRes(shadowmapRes)
            , enableVr(enableVr) {
        }

        int msaaLevel;
        int shadowmapRes;
        bool enableVr;
    };

    struct Global2DTextureSlot {
        vku::TextureImage2D tex;
        bool present;
    };

    struct GlobalCubeTextureSlot {
        vku::TextureImageCube tex;
        bool present;
    };

    struct RenderTextureResource {
        vku::GenericImage image;
        vk::ImageAspectFlagBits aspectFlags;
    };

    struct SubmeshInfo {
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    struct LoadedMeshData {
        vku::VertexBuffer vb;
        vku::IndexBuffer ib;
        uint32_t indexCount;
        vk::IndexType indexType;
        SubmeshInfo submeshes[NUM_SUBMESH_MATS];
        uint8_t numSubmeshes;
    };

    struct RenderCtx {
        RenderCtx(
            vk::UniqueCommandBuffer& cmdBuf,
            entt::registry& reg,
            uint32_t imageIndex,
            Camera& cam,
            std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources,
            uint32_t width, uint32_t height,
            std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes)
            : cmdBuf(cmdBuf)
            , reg(reg)
            , imageIndex(imageIndex)
            , cam(cam)
            , reuploadMats(false)
            , rtResources(rtResources)
            , loadedMeshes(loadedMeshes)
            , width(width)
            , height(height)
            , enableVR(false) {
        }

        vk::UniqueCommandBuffer& cmdBuf;
        vk::PipelineCache pipelineCache;

        entt::registry& reg;
        uint32_t imageIndex;
        Camera& cam;
        std::unique_ptr<TextureSlots>* textureSlots;
        std::unique_ptr<MaterialSlots>* materialSlots;
        std::unique_ptr<CubemapSlots>* cubemapSlots;
        bool reuploadMats;
        std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
        std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes;
        uint32_t width, height;
        glm::mat4 vrViewMats[2];
        glm::mat4 vrProjMats[2];
        glm::vec3 viewPos;
        bool enableVR;
#ifdef TRACY_ENABLE
        std::vector<TracyVkCtx>* tracyContexts;
#endif
    };

    struct PassSetupCtx {
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        vk::PipelineCache pipelineCache;
        vk::DescriptorPool descriptorPool;
        // Please only use the pool passed here for immediately executing commands during the setup phase.
        vk::CommandPool commandPool;
        vk::Instance instance;
        VmaAllocator allocator;
        uint32_t graphicsQueueFamilyIdx;
        GraphicsSettings graphicsSettings;
        std::unique_ptr<TextureSlots>* globalTexArray;
        std::unique_ptr<CubemapSlots>* cubemapSlots;
        std::unique_ptr<MaterialSlots>* materialSlots;
        std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
        int swapchainImageCount;
        bool enableVR;
        vku::GenericImage* brdfLut;
    };

    // Holds handles to useful Vulkan objects
    struct VulkanCtx {
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
        uint32_t renderWidth, renderHeight;
    };

    class XRInterface;

    struct RendererInitInfo {
        SDL_Window* window;
        std::vector<std::string> additionalInstanceExtensions;
        std::vector<std::string> additionalDeviceExtensions;
        bool enableVR;
        VrApi activeVrApi;
        IVRInterface* vrInterface;
        bool enablePicking;
    };

    class BRDFLUTRenderer {
    public:
        BRDFLUTRenderer(VulkanCtx& ctx);
        void render(VulkanCtx& ctx, vku::GenericImage& target);
    private:
        vk::UniqueRenderPass renderPass;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;

        vku::ShaderModule vs;
        vku::ShaderModule fs;
    };

    class CubemapConvoluter {
    public:
        CubemapConvoluter(std::shared_ptr<VulkanCtx> ctx);
        void convolute(vku::TextureImageCube& cubemap);
    private:
        vku::ShaderModule cs;
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniqueDescriptorSetLayout dsl;
        std::shared_ptr<VulkanCtx> vkCtx;
        vk::UniqueSampler sampler;
    };

    typedef uint32_t RTTPassHandle;
    struct RTTPassCreateInfo {
        uint32_t width, height;
    };

    class VKRenderer {
        const static uint32_t NUM_TEX_SLOTS = 64;
        const static uint32_t NUM_MAT_SLOTS = 256;
        const static uint32_t NUM_CUBEMAP_SLOTS = 64;

        struct RTTPassInternal {
            GraphSolver graphSolver;
            uint32_t width, height;
            RenderImageHandle hdrTarget;
            RenderImageHandle sdrFinalTarget;
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
        uint32_t computeQueueFamilyIdx;
        uint32_t presentQueueFamilyIdx;
        uint32_t asyncComputeQueueFamilyIdx;
        uint32_t width, height;
        vk::SampleCountFlagBits msaaSamples;
        int32_t numMSAASamples;
        vk::UniqueRenderPass imguiRenderPass;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        vk::UniqueSemaphore imageAcquire;
        vk::UniqueSemaphore commandComplete;
        vk::UniqueCommandPool commandPool;
        std::vector<vk::UniqueCommandBuffer> cmdBufs;
        std::vector<vk::Semaphore> cmdBufferSemaphores;
        std::vector<uint64_t> cmdBufSemaphoreVals;
        VmaAllocator allocator;

        // stuff related to standard geometry rendering
        RenderImageHandle depthStencilImage;
        RenderImageHandle polyImage;

        RenderImageHandle finalPrePresent;
        // openvr doesn't support presenting image layers
        // copy to another image
        RenderImageHandle finalPrePresentR;

        // shadowmapping stuff
        RenderImageHandle shadowmapImage;

        RenderImageHandle imguiImage;

        std::vector<vk::DescriptorSet> descriptorSets;
        SDL_Window* window;
        vk::UniqueQueryPool queryPool;
        uint64_t lastRenderTimeTicks;
        float timestampPeriod;

        std::unordered_map<RenderImageHandle, RenderTextureResource> rtResources;
        std::unordered_map<RTTPassHandle, RTTPassInternal> rttPasses;
        RenderImageHandle lastHandle;

        struct RTResourceCreateInfo {
            vk::ImageCreateInfo ici;
            vk::ImageViewType viewType;
            vk::ImageAspectFlagBits aspectFlags;
        };

        void imageBarrier(vk::CommandBuffer& cb, ImageBarrier& ib);
        RenderImageHandle createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName = nullptr);
        void createSwapchain(vk::SwapchainKHR oldSwapchain);
        void createFramebuffers();
        void createSCDependents();
        void presentNothing(uint32_t imageIndex);
        vku::ShaderModule loadShaderAsset(AssetID id);
        void acquireSwapchainImage(uint32_t* imageIdx);
        void createInstance(const RendererInitInfo& initInfo);
        void serializePipelineCache();
        void submitToOpenVR();
        void uploadSceneAssets(entt::registry& reg, RenderCtx& rCtx);

        std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
        int frameIdx;
        std::vector<TracyVkCtx> tracyContexts;
        std::unique_ptr<TextureSlots> texSlots;
        std::unique_ptr<MaterialSlots> matSlots;
        std::unique_ptr<CubemapSlots> cubemapSlots;

        GraphSolver graphSolver;
        uint32_t shadowmapRes;
        bool enableVR;
        PolyRenderPass* currentPRP;
        ImGuiRenderPass* irp;
        uint32_t renderWidth, renderHeight;
        IVRInterface* vrInterface;
        VrApi vrApi;
        float vrPredictAmount;
        bool clearMaterialIndices;
        bool isMinimised;
        bool useVsync;
        vku::GenericImage brdfLut;
        std::unique_ptr<CubemapConvoluter> cubemapConvoluter;
        ConVar lowLatencyMode;
        bool swapchainRecreated;
        bool enablePicking;
    public:
        double time;
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        void recreateSwapchain();
        void frame(Camera& cam, entt::registry& reg);
        void preloadMesh(AssetID id);
        void uploadProcObj(ProceduralObject& procObj);
        void requestEntityPick();
        void unloadUnusedMaterials(entt::registry& reg);
        void reloadMatsAndTextures();
        bool getPickedEnt(entt::entity* entOut);
        float getLastRenderTime() const { return lastRenderTimeTicks * timestampPeriod; }
        void setVRPredictAmount(float amt) { vrPredictAmount = amt; }
        void setVsync(bool vsync) { if (useVsync != vsync) { useVsync = vsync; recreateSwapchain(); } }
        bool getVsync() const { return useVsync; }

        ~VKRenderer();
    };
}