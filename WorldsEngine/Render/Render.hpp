#pragma once
#include <glm/glm.hpp>
#include "../VR/IVRInterface.hpp"
#include "Core/Engine.hpp"
#include "glm/common.hpp"
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
        glm::mat4 views[2];
        glm::mat4 projections[2];
        glm::vec4 viewPos[2];
    };

    struct PackedLight {
        glm::vec4 pack0;
        glm::vec4 pack1;
        glm::vec4 pack2;
    };

    struct ProxyAOComponent {
        glm::vec3 bounds;
    };

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
            pack2 = glm::vec4(m4[2][2], glm::vec3{m4[3]});
        }

        void setScale(glm::vec3 s) {
            pack3 = glm::vec4(s, pack3.w);
        }

        void setEntityId(uint32_t id) {
            pack3.w = glm::uintBitsToFloat(id);
        }
    };

    struct LightUB {
        glm::vec4 pack0;
        glm::vec4 pack1;
        glm::mat4 shadowmapMatrices[3];
        AOBox box[16];
        PackedLight lights[128];
    };

    struct QueueFamilyIndices {
        uint32_t graphics;
        uint32_t present;
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
        int numTriangles;
        double shadowmapGpuTime;
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
        RenderTexture(VulkanHandles* ctx, RTResourceCreateInfo resourceCreateInfo, const char* debugName);
        friend class VKRenderer;
    };

    struct ShadowCascadeInfo {
        glm::mat4 matrices[3];
        float texelsPerUnit[3];
    };

    struct RenderResources {
        TextureSlots& textures;
        CubemapSlots& cubemaps;
        MaterialSlots& materials;
        std::unordered_map<AssetID, LoadedMeshData>& meshes;
        vku::GenericImage* brdfLut;
        vku::GenericBuffer* materialBuffer;
        vku::GenericBuffer* vpMatrixBuffer;
    };

    struct RenderDebugContext {
        RenderDebugStats* stats;
#ifdef TRACY_ENABLE
        std::vector<TracyVkCtx>* tracyContexts;
#endif
    };

    struct PassSettings {
        bool enableVR;
        bool enableShadows;
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
        uint32_t frameIdx, lastFrameIdx;
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
        void calculateCascadeMatrices(entt::registry& world, Camera& cam, RenderContext& rCtx);
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
        VulkanHandles* getHandles() { return &handles; }
        const RenderDebugStats& getDebugStats() const { return dbgStats; }
        void uploadSceneAssets(entt::registry& reg);
        RenderResources getResources() {
            return RenderResources {
                *texSlots,
                *cubemapSlots,
                *matSlots,
                loadedMeshes
            };
        }

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
