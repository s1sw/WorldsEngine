#pragma once
#include <R2/R2.hpp>
#include <Render/DebugLines.hpp>
#include <Render/Render.hpp>
#include <robin_hood.h>
#include <Util/UniquePtr.hpp>
#include <mutex>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
struct ImDrawData;

namespace R2
{
    class BindlessTextureManager;
    class SubAllocatedBuffer;
    VK_DEFINE_HANDLE(SubAllocationHandle)
}

namespace R2::VK
{
    class Swapchain;
    class Event;
    class Fence;
    class Texture;
    class Buffer;
    class CommandBuffer;
    class TimestampPool;
    class Pipeline;
    class PipelineLayout;
}

typedef struct VkPhysicalDeviceProperties VkPhysicalDeviceProperties;
typedef struct VkPipelineCacheCreateInfo VkPipelineCacheCreateInfo;
VK_DEFINE_HANDLE(VkPipelineCache)
VK_DEFINE_HANDLE(VkDevice)
#undef VK_DEFINE_HANDLE

namespace enki
{
    class ITaskSet;
}

namespace worlds
{
    class VKRenderer;

    struct AOBox
    {
        glm::vec4 pack0, pack1, pack2, pack3;

        void setRotationMat(glm::mat3 mat)
        {
            pack0 = glm::vec4(mat[0][0], mat[0][1], mat[0][2], mat[1][0]);
            pack1 = glm::vec4(mat[1][1], mat[1][2], mat[2][0], mat[2][1]);
            pack2 = glm::vec4(mat[2][2], pack2.y, pack2.z, pack2.w);
        }

        void setTranslation(glm::vec3 t)
        {
            pack2 = glm::vec4(pack2.x, t.x, t.y, t.z);
        }

        void setMatrix(glm::mat4 m4)
        {
            pack0 = glm::vec4(m4[0][0], m4[0][1], m4[0][2], m4[1][0]);
            pack1 = glm::vec4(m4[1][1], m4[1][2], m4[2][0], m4[2][1]);
            pack2 = glm::vec4(m4[2][2], glm::vec3{m4[3]});
        }

        void setScale(glm::vec3 s)
        {
            pack3 = glm::vec4(s, pack3.w);
        }

        void setEntityId(uint32_t id)
        {
            pack3.w = glm::uintBitsToFloat(id);
        }
    };

    struct AOSphere
    {
        glm::vec3 position;
        float radius;
    };

    struct GPUCubemap
    {
        glm::vec3 extent;
        uint32_t texture;
        glm::vec3 position;
        uint32_t flags;
        float blendDistance;
    };

    struct LightUB
    {
        static const int MAX_LIGHTS = 64;
        static int LIGHT_TILE_SIZE;
        glm::mat4 additionalShadowMatrices[NUM_SHADOW_LIGHTS];
        uint32_t lightCount;
        uint32_t cubemapCount;
        uint32_t shadowmapIds[NUM_SHADOW_LIGHTS];
        glm::mat4 cascadeMatrices[4];
        PackedLight lights[MAX_LIGHTS];
        GPUCubemap cubemaps[64];
    };

    struct ShadowCascadeInfo
    {
        bool shadowCascadeNeeded = false;
        glm::mat4 matrices[4];
        float texelsPerUnit[4];
    };

    struct RenderDebugContext
    {
        RenderDebugStats* stats;
    };

    struct MeshBone
    {
        glm::mat4 inverseBindPose;
        glm::mat4 transform;
        uint32_t parentIdx;
    };

    struct VertexSkinInfo
    {
        int boneIds[4];
        float weights[4];
    };

    enum class IndexType
    {
        Uint16,
        Uint32
    };

    struct RenderSubmeshInfo
    {
        uint32_t indexCount;
        uint32_t indexOffset;
        uint8_t materialIndex;
    };

    struct RenderMeshInfo
    {
        R2::SubAllocationHandle vertexAllocationHandle;
        R2::SubAllocationHandle indexAllocationHandle;
        R2::SubAllocationHandle skinInfoAllocationHandle;

        // All of these offsets are in units of bytes!!
        // If you want to index into the buffer, make sure to divide
        // by size first
        uint32_t vertsOffset;
        uint32_t indexOffset;
        uint32_t skinInfoOffset;

        uint32_t numVertices;

        uint8_t numSubmeshes;
        RenderSubmeshInfo submeshInfo[NUM_SUBMESH_MATS];

        float boundingSphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    class RenderMeshManager
    {
    public:
        RenderMeshManager(R2::VK::Core* core);
        ~RenderMeshManager();

        R2::VK::Buffer* getVertexBuffer();
        R2::VK::Buffer* getIndexBuffer();
        R2::VK::Buffer* getSkinInfoBuffer();

        RenderMeshInfo& loadOrGet(AssetID id);
        bool get(AssetID id, RenderMeshInfo** rmi);
        uint64_t getSkinnedVertsOffset() const;

    private:
        robin_hood::unordered_node_map<AssetID, RenderMeshInfo> meshes;
        R2::SubAllocatedBuffer* vertexBuffer;
        R2::SubAllocatedBuffer* indexBuffer;
        R2::SubAllocatedBuffer* skinInfoBuffer;
        R2::VK::Core* core;

        R2::SubAllocationHandle skinnedVertsAllocation;
        uint64_t skinnedVertsOffset;

        void loadToRMI(AssetID asset, RenderMeshInfo& meshInfo);
    };

    class VKTextureManager
    {
    public:
        VKTextureManager(R2::VK::Core* core, R2::BindlessTextureManager* textureManager);
        ~VKTextureManager();
        uint32_t loadAndGetAsync(AssetID id);
        enki::ITaskSet* loadAsync(AssetID id);
        uint32_t loadSynchronous(AssetID id);
        uint32_t get(AssetID id);
        bool isLoaded(AssetID id);
        void unload(AssetID id);
        void release(AssetID id);
        void showDebugMenu();

    private:
        struct TextureLoadTask;
        struct TexInfo
        {
            R2::VK::Texture* tex;
            uint32_t bindlessId;
            int refCount;
            bool isCubemap;
        };

        uint32_t load(AssetID id, uint32_t handle);
        R2::VK::Core* core;
        R2::BindlessTextureManager* textureManager;
        robin_hood::unordered_node_map<AssetID, TexInfo> textureIds;
        std::mutex idMutex;
        uint32_t missingTextureID;
        R2::VK::Texture* missingTexture;
    };

    class VKUITextureManager : public IUITextureManager
    {
        VKTextureManager* texMan;

    public:
        VKUITextureManager(VKTextureManager* texMan);
        ImTextureID loadOrGet(AssetID id) override;
        void unload(AssetID id) override;
    };

    class IRenderPipeline;
    class VKRTTPass : public RTTPass
    {
        friend class VKRenderer;

        VKRenderer* renderer;
        VKRTTPass(VKRenderer* renderer, const RTTPassSettings& ci, IRenderPipeline* pipeline);
        ~VKRTTPass() override;

        R2::VK::Texture* finalTarget;
        uint32_t finalTargetBindlessID;
        Camera* cam;
        IRenderPipeline* pipeline;
        RTTPassSettings settings;
        bool hdrDataRequested = false;
        bool hdrDataReady = false;
        R2::VK::Texture* requestedHdrResult;
        R2::VK::Buffer* hdrDataBuffer;
        R2::VK::Event* captureEvent;

        void downloadHDROutput(R2::VK::CommandBuffer& cb);

    public:
        void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix) override;

        void requestHDRData() override;
        bool getHDRData(float*& out) override;
        void resize(int newWidth, int newHeight) override;
        ImTextureID getUITextureID() override;
        const RTTPassSettings& getSettings();

        R2::VK::Texture* getFinalTarget();
        Camera* getCamera();
    };

    class XRPresentManager
    {
        int width, height;
        const EngineInterfaces& interfaces;
        R2::VK::Core* core;
    public:
        XRPresentManager(VKRenderer* renderer, const EngineInterfaces& interfaces, int width, int height);
        void copyFromLayered(R2::VK::CommandBuffer cb, R2::VK::Texture* layeredTexture);
        void beginFrame();
        void waitFrame();
        void endFrame();
    };

    class ShadowmapManager
    {
        struct ShadowmapInfo
        {
            UniquePtr<R2::VK::Texture> texture;
            uint32_t bindlessID;
        };

        VKRenderer* renderer;
        std::vector<ShadowmapInfo> shadowmapInfo;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        std::vector<glm::mat4> shadowmapMatrices;
    public:
        ShadowmapManager(VKRenderer* renderer);
        void AllocateShadowmaps(entt::registry& registry);
        void RenderShadowmaps(R2::VK::CommandBuffer& cb, entt::registry& registry, glm::mat4& viewMatrix);
        glm::mat4& GetShadowVPMatrix(uint32_t idx);
        uint32_t GetShadowmapId(uint32_t idx);
    };

    class ObjectPickPass;
    class ParticleDataManager;
    class ParticleSimulator;

    class VKRenderer : public Renderer
    {
        const EngineInterfaces& interfaces;
        R2::VK::Core* core;
        R2::VK::Swapchain* swapchain;
        R2::VK::Fence* frameFence;
        R2::BindlessTextureManager* bindlessTextureManager;
        VKUITextureManager* uiTextureManager;
        VKTextureManager* textureManager;
        RenderMeshManager* renderMeshManager;
        UniquePtr<XRPresentManager> xrPresentManager;
        glm::mat4 vrUsedPose;
        UniquePtr<R2::VK::TimestampPool> timestampPool;
        UniquePtr<ShadowmapManager> shadowmapManager;
        UniquePtr<ObjectPickPass> objectPickPass;
        UniquePtr<ParticleDataManager> particleDataManager;
        UniquePtr<ParticleSimulator> particleSimulator;

        std::vector<VKRTTPass*> rttPasses;

        ImDrawData* imguiDrawData;

        RenderDebugStats debugStats;
        float lastGPUTime;
        float timestampPeriod;
        double timeAccumulator;
        friend class VKRTTPass;
        const DebugLine* currentDebugLines;
        size_t currentDebugLineCount;

    public:
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        ~VKRenderer() override;

        void frame(entt::registry& reg, float deltaTime) override;

        float getLastGPUTime() const override;
        void setVRUsedPose(glm::mat4 pose) override;

        void setVsync(bool vsync) override;
        bool getVsync() const override;

        RenderDebugStats& getDebugStats() override;
        IUITextureManager* getUITextureManager() override;

        void setImGuiDrawData(void* drawData) override;

        RTTPass* createRTTPass(RTTPassSettings& ci) override;
        void destroyRTTPass(RTTPass* pass) override;

        void requestPick(PickParams params) override;
        bool getPickResult(uint32_t& entityId) override;

        void reloadShaders() override;
        void drawDebugMenus();

        R2::VK::Core* getCore();
        RenderMeshManager* getMeshManager();
        R2::BindlessTextureManager* getBindlessTextureManager();
        VKTextureManager* getTextureManager();
        ShadowmapManager* getShadowmapManager();
        ParticleDataManager* getParticleDataManager();
        const DebugLine* getCurrentDebugLines(size_t* count);
        double getTime();
    };
}
