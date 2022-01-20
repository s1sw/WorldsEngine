#pragma once
#include "Render.hpp"
#include <Render/vku/vku.hpp>
#include <Render/vku/DebugCallback.hpp>
#include <Render/ResourceSlots.hpp>
#include <tracy/TracyVulkan.hpp>
#include <robin_hood.h>
#include <deque>
#include <mutex>

struct ImDrawData;

namespace std {
    class mutex;
}

namespace worlds {
    class PolyRenderPass;
    class ImGuiRenderPass;
    class TonemapFXRenderPass;
    class ShadowCascadePass;
    class AdditionalShadowsPass;
    class GTAORenderPass;
    class VKRenderer;

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
        static const int MAX_LIGHTS = 256;
        glm::mat4 additionalShadowMatrices[NUM_SHADOW_LIGHTS];
        float numLights;
        float cascadeTexelsPerUnit[3];
        glm::vec4 pack1;
        glm::mat4 shadowmapMatrices[3];
        PackedLight lights[256];
        AOBox box[16];
        AOSphere sphere[16];
        uint32_t sphereIds[16];
    };

    struct ModelMatrices {
        static const uint32_t SIZE = 2048;
        glm::mat4 modelMatrices[2048];
    };

    struct MaterialsUB {
        PackedMaterial materials[256];
    };

    struct ShadowCascadeInfo {
        bool shadowCascadeNeeded = false;
        glm::mat4 matrices[3];
        float texelsPerUnit[3];
    };

    struct RenderDebugContext {
        RenderDebugStats* stats;
#ifdef TRACY_ENABLE
        std::vector<TracyVkCtx>* tracyContexts;
#endif
    };

    struct MeshBone {
        glm::mat4 inverseBindPose;
        glm::mat4 transform;
        uint32_t parentIdx;
        std::string name;
    };

    struct VertSkinningInfo {
        int boneIds[4];
        float weights[4];
    };

    struct LoadedMeshData {
        vku::VertexBuffer vb;
        vku::IndexBuffer ib;

        bool isSkinned;
        vku::VertexBuffer vertexSkinWeights;
        std::vector<MeshBone> meshBones;
        std::vector<uint32_t> boneUpdateOrder;

        uint32_t indexCount;
        VkIndexType indexType;
        SubmeshInfo submeshes[NUM_SUBMESH_MATS];
        uint8_t numSubmeshes;
        float sphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    struct QueueFamilies {
        uint32_t graphics;
        uint32_t present;
        uint32_t asyncCompute;
    };

    struct Queues {
        VkQueue graphics;
        VkQueue present;
        VkQueue asyncCompute;

        uint32_t graphicsIdx;
        uint32_t presentIdx;
        uint32_t asyncComputeIdx;
    };

    class Swapchain {
    public:
        Swapchain(VkPhysicalDevice&, VkDevice, VkSurfaceKHR&, const Queues& queues, bool fullscreen, VkSwapchainKHR oldSwapchain = VkSwapchainKHR(), VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR);
        ~Swapchain();
        void getSize(uint32_t* x, uint32_t* y) { *x = width; *y = height; }
        VkResult acquireImage(VkDevice device, VkSemaphore semaphore, uint32_t* imageIndex);
        VkSwapchainKHR& getSwapchain() { return swapchain; }
        VkFormat imageFormat() { return format; }
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
    private:
        VkDevice device;
        VkSwapchainKHR swapchain;
        VkFormat format;
        uint32_t width;
        uint32_t height;
    };

    // Holds handles to useful Vulkan objects
    struct VulkanHandles {
        VKVendor vendor;
        bool hasOutOfOrderRasterization;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkPipelineCache pipelineCache;
        VkDescriptorPool descriptorPool;
        VkCommandPool commandPool;
        VkInstance instance;
        VmaAllocator allocator;
        uint32_t graphicsQueueFamilyIdx;
        GraphicsSettings graphicsSettings;
        uint32_t width, height;
        uint32_t vrWidth, vrHeight;
    };

    enum class TextureType {
        T1D,
        T2D,
        T2DArray,
        T3D,
        TCube
    };

    enum class SharingMode {
        Exclusive,
        Concurrent
    };

    struct TextureResourceCreateInfo {
        TextureResourceCreateInfo() {}

        TextureResourceCreateInfo(TextureType type, VkFormat format,
            int width, int height, VkImageUsageFlags usage)
            : type(type)
            , format(format)
            , width(width)
            , height(height)
            , usage(usage)
            , initialLayout(VK_IMAGE_LAYOUT_UNDEFINED) {

            if (format > 124 && format < 130) {
                aspectFlags = (VkImageAspectFlags)0;
                if (format != VK_FORMAT_S8_UINT)
                    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

                if (format >= 127)
                    aspectFlags &= VK_IMAGE_ASPECT_STENCIL_BIT;
            } else {
                aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        TextureType type;
        VkFormat format;

        int width;
        int height;
        int depth = 1;
        int mipLevels = 1;
        int layers = 1;
        int samples = 1;

        VkImageUsageFlags usage;
        VkImageLayout initialLayout;
        SharingMode sharingMode = SharingMode::Exclusive;

        VkImageAspectFlags aspectFlags;
    };

    enum class ResourceType {
        Buffer,
        Image
    };

    // A class for either a buffer or an image.
    struct RenderResource {
        ResourceType type;
        std::unique_ptr<vku::Resource> resource;
        std::string name;

        vku::GenericImage& image() {
            return *static_cast<vku::GenericImage*>(resource.get());
        }

        vku::GenericBuffer& buffer() {
            return *static_cast<vku::GenericBuffer*>(resource.get());
        }
    };

    struct RenderResources {
        TextureSlots& textures;
        CubemapSlots& cubemaps;
        MaterialSlots& materials;
        robin_hood::unordered_map<AssetID, LoadedMeshData>& meshes;
        vku::GenericImage* brdfLut;
        vku::GenericBuffer* materialBuffer;
        vku::GenericBuffer* vpMatrixBuffer;
        RenderResource* shadowCascades;
        RenderResource** additionalShadowImages;
    };

    struct RenderContext {
        RenderResources resources;
        ShadowCascadeInfo cascadeInfo;
        RenderDebugContext debugContext;
        GraphicsSettings passSettings;
        entt::registry& registry;
        VKRenderer* renderer;

        glm::mat4 projMatrices[2];
        glm::mat4 viewMatrices[2];

        VkCommandBuffer cmdBuf;
        uint32_t passWidth;
        uint32_t passHeight;
        uint32_t frameIndex;
        int maxSimultaneousFrames;
    };

    class BRDFLUTRenderer {
    public:
        BRDFLUTRenderer(VulkanHandles& ctx);
        void render(VulkanHandles& ctx, vku::GenericImage& target);
    private:
        vku::RenderPass renderPass;
        vku::Pipeline pipeline;
        vku::PipelineLayout pipelineLayout;

        vku::ShaderModule vs;
        vku::ShaderModule fs;
    };

    class CubemapConvoluter {
    public:
        CubemapConvoluter(std::shared_ptr<VulkanHandles> ctx);
        void convolute(vku::TextureImageCube& cubemap);
    private:
        vku::ShaderModule cs;
        vku::Pipeline pipeline;
        vku::PipelineLayout pipelineLayout;
        vku::DescriptorSetLayout dsl;
        std::shared_ptr<VulkanHandles> vkCtx;
        vku::Sampler sampler;
    };

    class PipelineCacheSerializer {
    public:
        static void loadPipelineCache(const VkPhysicalDeviceProperties&, VkPipelineCacheCreateInfo&);
        static void savePipelineCache(const VkPhysicalDeviceProperties&, const VkPipelineCache&, const VkDevice&);
    };

    class DeletionQueue {
    public:
        static void queueObjectDeletion(void* object, VkObjectType type);
        static void queueMemoryFree(VmaAllocation allocation);
        static void queueDescriptorSetFree(VkDescriptorPool dPool, VkDescriptorSet ds);
        static void setCurrentFrame(uint32_t frame);
        static void cleanupFrame(uint32_t frame);
        static void resize(uint32_t maxFrames);
    private:
        struct ObjectDeletion {
            void* object;
            VkObjectType type;
        };

        struct MemoryFree {
            VmaAllocation allocation;
        };

        struct DescriptorSetFree {
            VkDescriptorPool desciptorPool;
            VkDescriptorSet descriptorSet;
        };

        struct DQueue {
            std::deque<ObjectDeletion> objectDeletions;
            std::deque<MemoryFree> memoryFrees;
            std::deque<DescriptorSetFree> dsFrees;
        };

        static std::vector<DQueue> deletionQueues;
        static uint32_t currentFrameIndex;

        static void processObjectDeletion(const ObjectDeletion& od);
        static void processMemoryFree(const MemoryFree& mf);
    };

    class VKRTTPass : public RTTPass {
    public:
        void drawNow(entt::registry& world) override;
        RenderResource* hdrTarget;
        RenderResource* sdrFinalTarget;
        RenderResource* depthTarget;
        RenderResource* bloomTarget;
        void requestPick(int x, int y) override;
        bool getPickResult(uint32_t* result) override;
        float* getHDRData() override;
    private:
        VKRTTPass(const RTTPassCreateInfo& ci, VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats);
        ~VKRTTPass();
        void create(VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats);
        void destroy();
        PolyRenderPass* prp;
        TonemapFXRenderPass* trp;
        bool isVr;
        bool outputToScreen;
        bool enableShadows;
        Camera* cam;
        VKRenderer* renderer;
        IVRInterface* vrInterface;
        RenderDebugStats* dbgStats;
        void writeCmds(uint32_t frameIdx, VkCommandBuffer buf, entt::registry& world);
        vku::DescriptorPool descriptorPool;
        RTTPassCreateInfo createInfo;
        GraphicsSettings passSettings;

        friend class VKRenderer;
    };

    class VKUITextureManager : public IUITextureManager {
    public:
        VKUITextureManager(const VulkanHandles& handles);
        ImTextureID loadOrGet(AssetID id) override;
        void unload(AssetID id) override;
        ~VKUITextureManager();
    private:
        struct UITexInfo {
            VkDescriptorSet ds;
            vku::TextureImage2D image;
        };

        UITexInfo* load(AssetID id);
        const VulkanHandles& handles;
        robin_hood::unordered_map<AssetID, UITexInfo*> texInfo;
    };

    class VKPresentSubmitManager {
    public:
        VKPresentSubmitManager(SDL_Window* window, VkSurfaceKHR surface, VulkanHandles* handles, Queues* queues, RenderDebugStats* dbgStats);
        void recreateSwapchain(bool useVsync, uint32_t& width, uint32_t& height);
        int acquireFrame(VkCommandBuffer& cmdBuf, int& imageIndex);
        void submit();
        void present();
        void presentNothing();
        int numFramesInFlight();
        Swapchain& currentSwapchain();
#ifdef TRACY_ENABLE
        void setupTracyContexts(std::vector<TracyVkCtx>& tracyContexts);
#endif
    private:
        std::vector<VkCommandBuffer> cmdBufs;
        int maxFramesInFlight = 2;
        int currentFrame = 0;
        int currentImage = 0;
        std::vector<VkSemaphore> cmdBufferSemaphores;
        std::vector<VkSemaphore> imgAvailable;
        std::vector<VkFence> cmdBufFences;
        std::vector<VkFence> imgFences;
        Swapchain* sc;
        RenderDebugStats* dbgStats;
        VulkanHandles* handles;
        Queues* queues;
        SDL_Window* window;
        VkSurfaceKHR surface;
        std::mutex swapchainMutex;
    };

    class VKRenderer : public Renderer {
        const static uint32_t NUM_TEX_SLOTS = 256;
        const static uint32_t NUM_MAT_SLOTS = 256;
        const static uint32_t NUM_CUBEMAP_SLOTS = 64;

        struct RTTPassInternal {
            PolyRenderPass* prp;
            TonemapFXRenderPass* trp;
            uint32_t width, height;
            RenderResource* hdrTarget;
            RenderResource* sdrFinalTarget;
            RenderResource* depthTarget;
            bool isVr;
            bool outputToScreen;
            bool enableShadows;
            bool active;
            Camera* cam;
        };

        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkPipelineCache pipelineCache;
        VkDescriptorPool descriptorPool;

        Queues queues;

        VkSurfaceKHR surface;
        vku::DebugCallback dbgCallback;

        uint32_t width, height;
        VkSampleCountFlagBits msaaSamples;
        int32_t numMSAASamples;
        std::vector<vku::Framebuffer> framebuffers;
        VkCommandPool commandPool;

        std::unique_ptr<VKPresentSubmitManager> presentSubmitManager;
        VmaAllocator allocator;
        vku::GenericBuffer materialUB;
        vku::GenericBuffer vpBuffer;
        VulkanHandles handles;

        RenderResource* finalPrePresent;

        RenderResource* leftEye;
        RenderResource* rightEye;

        RenderResource* shadowmapImage;
        RenderResource* shadowImages[NUM_SHADOW_LIGHTS];
        RenderResource* imguiImage;

        SDL_Window* window;
        VkQueryPool queryPool;
        uint64_t lastRenderTimeTicks;
        float timestampPeriod;

        std::vector<VKRTTPass*> rttPasses;
        robin_hood::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
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
        VKUITextureManager* uiTextureMan;

        void createFramebuffers();
        void createSCDependents();
        vku::ShaderModule loadShaderAsset(AssetID id);
        void createInstance(const RendererInitInfo& initInfo);
        void submitToOpenVR();
        glm::mat4 getCascadeMatrix(bool forVr, Camera cam, glm::vec3 lightdir, glm::mat4 frustumMatrix, float& texelsPerUnit);
        void calculateCascadeMatrices(bool forVr, entt::registry& world, Camera& cam, RenderContext& rCtx);
        void writeCmdBuf(VkCommandBuffer cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg);
        void reuploadMaterials();
        ImDrawData* imDrawData;
        std::mutex* apiMutex;
        void recreateSwapchainInternal(int newWidth = -1, int newHeight = -1);
        void createSpotShadowImages();
        void createCascadeShadowImages();

        friend class VKRTTPass;
    public:
        double time;
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        void recreateSwapchain(int newWidth = -1, int newHeight = -1) override;
        void frame(Camera& cam, entt::registry& reg) override;
        void preloadMesh(AssetID id) override;
        void unloadUnusedMaterials(entt::registry& reg) override;
        void reloadContent(ReloadFlags flags) override;
        float getLastRenderTime() const override { return lastRenderTimeTicks * timestampPeriod; }
        void setVRPredictAmount(float amt) override { vrPredictAmount = amt; }
        void setVsync(bool vsync) override;
        bool getVsync() const override { return useVsync; }
        VulkanHandles* getHandles() { return &handles; }
        const RenderDebugStats& getDebugStats() const override { return dbgStats; }
        void uploadSceneAssets(entt::registry& reg) override;
        RenderResources getResources();

        void setImGuiDrawData(void* drawData) override;

        VKRTTPass* createRTTPass(RTTPassCreateInfo& ci) override;
        void destroyRTTPass(RTTPass* pass) override;

        RenderResource* createTextureResource(TextureResourceCreateInfo resourceCreateInfo, const char* debugName = nullptr);

        void triggerRenderdocCapture() override;
        void startRdocCapture() override;
        void endRdocCapture() override;

        IUITextureManager& uiTextureManager() override;

        ~VKRenderer();
    };

    inline void addDebugLabel(VkCommandBuffer cmdBuf, const char* name, float r, float g, float b, float a) {
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name;
        label.color[0] = r;
        label.color[1] = g;
        label.color[2] = b;
        label.color[3] = a;

        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);
    }
}
