#pragma once
#include <glm/glm.hpp>
#include "../VR/IVRInterface.hpp"
#include "tracy/TracyVulkan.hpp"
#include "ResourceSlots.hpp"
#include "Camera.hpp"
#include "RenderGraph.hpp"
#include <SDL.h>
#include "../Core/Console.hpp"
#include <entt/entt.hpp>
#define NUM_SUBMESH_MATS 32

namespace worlds {
    const int NUM_SHADOW_LIGHTS = 1;
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 uv;
        glm::vec2 uv2;
    };

    struct WorldCubemap {
        AssetID cubemapId;
        glm::vec3 extent{0.0f};
        bool cubeParallax = false;
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
        glm::mat4 views[4];
        glm::mat4 projections[4];
        glm::vec4 viewPos[4];
    };

    struct PackedLight {
        glm::vec4 pack0;
        glm::vec4 pack1;
        glm::vec4 pack2;
    };

    struct LightUB {
        glm::vec4 pack0;
        glm::mat4 shadowmapMatrices[3];
        PackedLight lights[128];
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
    class TonemapRenderPass;
    class ShadowCascadePass;
    class GTAORenderPass;

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
        float sphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    struct RenderDebugStats {
        int numDrawCalls;
        int numCulledObjs;
        uint64_t vramUsage;
        int numRTTPasses;
        int numPipelineSwitches;
    };

    // Holds handles to useful Vulkan objects
    struct VulkanHandles {
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

    struct RTResourceCreateInfo {
        vk::ImageCreateInfo ici;
        vk::ImageViewType viewType;
        vk::ImageAspectFlagBits aspectFlags;
    };

    class RenderTexture{
    public:
        vku::GenericImage image;
        vk::ImageAspectFlagBits aspectFlags;
    private:
        RenderTexture(const VulkanHandles& ctx, RTResourceCreateInfo resourceCreateInfo, const char* debugName);
        friend class VKRenderer;
    };

    struct SlotArrays {
        TextureSlots& textures;
        CubemapSlots& cubemaps;
        MaterialSlots& materials;
    };

    struct RenderCtx {
        RenderCtx(
            vk::CommandBuffer cmdBuf,
            entt::registry& reg,
            uint32_t imageIndex,
            Camera* cam,
            SlotArrays slotArrays,
            uint32_t width, uint32_t height,
            std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes)
            : cmdBuf(cmdBuf)
            , reg(reg)
            , imageIndex(imageIndex)
            , cam(cam)
            , slotArrays(slotArrays)
            , loadedMeshes(loadedMeshes)
            , width(width)
            , height(height)
            , enableVR(false) {
        }

        vk::CommandBuffer cmdBuf;
        vk::PipelineCache pipelineCache;

        entt::registry& reg;
        uint32_t imageIndex;
        Camera* cam;
        SlotArrays slotArrays;
        std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes;
        uint32_t width, height;
        glm::mat4 vrViewMats[2];
        glm::mat4 vrProjMats[2];
        glm::vec3 viewPos;
        glm::mat4 cascadeShadowMatrices[3];
        float cascadeTexelsPerUnit[3];
        RenderTexture** shadowImages;
        bool enableVR;
#ifdef TRACY_ENABLE
        std::vector<TracyVkCtx>* tracyContexts;
#endif
        RenderDebugStats* dbgStats;
    };

    struct PassSetupCtx {
        vku::UniformBuffer* materialUB;
        VulkanHandles vkCtx;
        SlotArrays slotArrays;
        int swapchainImageCount;
        bool enableVR;
        vku::GenericImage* brdfLut;
        uint32_t passWidth;
        uint32_t passHeight;
    };

    struct RendererInitInfo {
        SDL_Window* window;
        std::vector<std::string> additionalInstanceExtensions;
        std::vector<std::string> additionalDeviceExtensions;
        bool enableVR;
        VrApi activeVrApi;
        IVRInterface* vrInterface;
        bool enablePicking;
        const char* applicationName = nullptr;
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

    typedef uint32_t RTTPassHandle;
    struct RTTPassCreateInfo {
        Camera* cam = nullptr;
        uint32_t width, height;
        bool isVr;
        bool useForPicking;
        bool enableShadows;
        bool outputToScreen;
    };

    class PipelineCacheSerializer {
    public:
        static void loadPipelineCache(const vk::PhysicalDeviceProperties&, vk::PipelineCacheCreateInfo&);
        static void savePipelineCache(const vk::PhysicalDeviceProperties&, const vk::PipelineCache&, const vk::Device&);
    };

    enum class ReloadFlags {
        Textures = 1,
        Materials = 2,
        Cubemaps = 4,
        Meshes = 8,
        All = 15
    };

    inline ReloadFlags operator|(ReloadFlags l, ReloadFlags r) {
        return (ReloadFlags)((uint32_t)l | (uint32_t)r);
    }

    class VKRenderer {
        const static uint32_t NUM_TEX_SLOTS = 256;
        const static uint32_t NUM_MAT_SLOTS = 256;
        const static uint32_t NUM_CUBEMAP_SLOTS = 64;

        struct RTTPassInternal {
            PolyRenderPass* prp;
            TonemapRenderPass* trp;
            GTAORenderPass* gtrp;
            uint32_t width, height;
            RenderTexture* hdrTarget;
            RenderTexture* sdrFinalTarget;
            RenderTexture* depthTarget;
            RenderTexture* gtaoOut;
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
        vku::UniformBuffer materialUB;
        VulkanHandles handles;

        RenderTexture* finalPrePresent;
        // openvr doesn't support presenting image layers
        // copy to another image
        RenderTexture* finalPrePresentR;

        RenderTexture* shadowmapImage;
        RenderTexture* shadowImages[NUM_SHADOW_LIGHTS];
        RenderTexture* imguiImage;

        std::vector<vk::DescriptorSet> descriptorSets;
        SDL_Window* window;
        vk::UniqueQueryPool queryPool;
        uint64_t lastRenderTimeTicks;
        float timestampPeriod;

        std::unordered_map<RTTPassHandle, RTTPassInternal> rttPasses;

        std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
        std::vector<TracyVkCtx> tracyContexts;
        std::unique_ptr<TextureSlots> texSlots;
        std::unique_ptr<MaterialSlots> matSlots;
        std::unique_ptr<CubemapSlots> cubemapSlots;

        uint32_t shadowmapRes;
        bool enableVR;
        PolyRenderPass* pickingPRP;
        PolyRenderPass* vrPRP;
        ImGuiRenderPass* irp;
        uint32_t renderWidth, renderHeight;
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
        RTTPassHandle vrPass;
        RTTPassHandle nextHandle;
        uint32_t frameIdx;
        ShadowCascadePass* shadowCascadePass;
        void* rdocApi;

        void createSwapchain(vk::SwapchainKHR oldSwapchain);
        void createFramebuffers();
        void createSCDependents();
        void presentNothing(uint32_t imageIndex);
        vku::ShaderModule loadShaderAsset(AssetID id);
        void createInstance(const RendererInitInfo& initInfo);
        void submitToOpenVR();
        glm::mat4 getCascadeMatrix(Camera cam, glm::vec3 lightdir, glm::mat4 frustumMatrix, float& texelsPerUnit);
        void calculateCascadeMatrices(entt::registry& world, RenderCtx& rCtx);
        void writeCmdBuf(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg);
        void writePassCmds(RTTPassHandle pass, vk::CommandBuffer cmdBuf, entt::registry& world);
        void reuploadMaterials();
    public:
        double time;
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        void recreateSwapchain();
        void frame(Camera& cam, entt::registry& reg);
        void preloadMesh(AssetID id);
        void uploadProcObj(ProceduralObject& procObj);
        void requestEntityPick(int x, int y);
        void unloadUnusedMaterials(entt::registry& reg);
        void reloadContent(ReloadFlags flags);
        bool getPickedEnt(entt::entity* entOut);
        float getLastRenderTime() const { return lastRenderTimeTicks * timestampPeriod; }
        void setVRPredictAmount(float amt) { vrPredictAmount = amt; }
        void setVsync(bool vsync) { if (useVsync != vsync) { useVsync = vsync; recreateSwapchain(); } }
        bool getVsync() const { return useVsync; }
        const RenderDebugStats& getDebugStats() const { return dbgStats; }
        const VulkanHandles& getVKCtx();
        void uploadSceneAssets(entt::registry& reg);

        RTTPassHandle createRTTPass(RTTPassCreateInfo& ci);
        // Pass to be late updated with new VR pose data
        void setVRPass(RTTPassHandle handle) { vrPass = handle; }
        void destroyRTTPass(RTTPassHandle handle);
        vku::GenericImage& getSDRTarget(RTTPassHandle handle) { return rttPasses.at(handle).sdrFinalTarget->image; }
        void setRTTPassActive(RTTPassHandle handle, bool active) { rttPasses.at(handle).active = active; }
        bool isPassValid(RTTPassHandle handle) { return rttPasses.find(handle) != rttPasses.end(); }
        float* getPassHDRData(RTTPassHandle handle);
        void updatePass(RTTPassHandle handle, entt::registry& world);
        RenderTexture* createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName = nullptr);

        void triggerRenderdocCapture();
        void startRdocCapture();
        void endRdocCapture();

        ~VKRenderer();
    };
}
