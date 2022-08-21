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
    class Fence;
    class Texture;
    class Buffer;
    class CommandBuffer;
}

typedef struct VkPhysicalDeviceProperties VkPhysicalDeviceProperties;
typedef struct VkPipelineCacheCreateInfo VkPipelineCacheCreateInfo;
VK_DEFINE_HANDLE(VkPipelineCache)
VK_DEFINE_HANDLE(VkDevice)
#undef VK_DEFINE_HANDLE

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
    };

    struct LightUB
    {
        static const int MAX_LIGHTS = 256;
        static int LIGHT_TILE_SIZE;
        glm::mat4 additionalShadowMatrices[NUM_SHADOW_LIGHTS];
        uint32_t lightCount;
        uint32_t aoBoxCount;
        uint32_t aoSphereCount;
        uint32_t cubemapCount;
        float cascadeTexelsPerUnit[4];
        glm::mat4 shadowmapMatrices[4];
        PackedLight lights[256];
        AOBox box[128];
        AOSphere sphere[16];
        uint32_t sphereIds[16];
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

    struct VertSkinningInfo
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

        uint32_t vertsOffset;
        uint32_t indexOffset;

        uint8_t numSubmeshes;
        RenderSubmeshInfo submeshInfo[NUM_SUBMESH_MATS];

        float boundingSphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    class RenderMeshManager
    {
        robin_hood::unordered_map<AssetID, RenderMeshInfo> meshes;
        R2::SubAllocatedBuffer* vertexBuffer;
        R2::SubAllocatedBuffer* indexBuffer;
        R2::VK::Core* core;

    public:
        RenderMeshManager(R2::VK::Core* core);
        ~RenderMeshManager();

        R2::VK::Buffer* getVertexBuffer();
        R2::VK::Buffer* getIndexBuffer();

        RenderMeshInfo& loadOrGet(AssetID id);
    };

    class VKTextureManager
    {
    public:
        VKTextureManager(R2::VK::Core* core, R2::BindlessTextureManager* textureManager);
        ~VKTextureManager();
        uint32_t loadAndGet(AssetID id);
        uint32_t get(AssetID id);
        bool isLoaded(AssetID id);
        void unload(AssetID id);
        void release(AssetID id);
        void showDebugMenu();

    private:
        struct TexInfo
        {
            R2::VK::Texture* tex;
            uint32_t bindlessId;
            int refCount;
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
        ~VKRTTPass();

        R2::VK::Texture* finalTarget;
        uint32_t finalTargetBindlessID;
        Camera* cam;
        IRenderPipeline* pipeline;
        RTTPassSettings settings;

    public:
        void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix) override;

        float* getHDRData() override;
        void resize(int newWidth, int newHeight) override;
        ImTextureID getUITextureID() override;
        const RTTPassSettings& getSettings();

        R2::VK::Texture* getFinalTarget();
        Camera* getCamera();
    };

    class XRPresentManager
    {
        UniquePtr<R2::VK::Texture> leftEye;
        UniquePtr<R2::VK::Texture> rightEye;
        int width, height;
        R2::VK::Core* core;
        void createTextures();

    public:
        XRPresentManager(R2::VK::Core* core, int width, int height);
        void resize(int width, int height);
        void copyFromLayered(R2::VK::CommandBuffer cb, R2::VK::Texture* layeredTexture);
        void submit(glm::mat4 usedPose);
    };

    class VKRenderer : public Renderer
    {
        R2::VK::Core* core;
        R2::VK::Swapchain* swapchain;
        R2::VK::Fence* frameFence;
        R2::BindlessTextureManager* bindlessTextureManager;
        VKUITextureManager* uiTextureManager;
        VKTextureManager* textureManager;
        RenderMeshManager* renderMeshManager;
        UniquePtr<XRPresentManager> xrPresentManager;
        glm::mat4 vrUsedPose;

        std::vector<VKRTTPass*> rttPasses;

        ImDrawData* imguiDrawData;

        RenderDebugStats debugStats;
        float lastGPUTime;
        friend class VKRTTPass;
        const DebugLine* currentDebugLines;
        size_t currentDebugLineCount;

    public:
        VKRenderer(const RendererInitInfo& initInfo, bool* success);
        ~VKRenderer();

        void frame(entt::registry& reg) override;

        float getLastGPUTime() const override;
        void setVRUsedPose(glm::mat4 pose) override;

        void setVsync(bool vsync) override;
        bool getVsync() const override;

        const RenderDebugStats& getDebugStats() const override;
        IUITextureManager* getUITextureManager() override;

        void setImGuiDrawData(void* drawData) override;

        RTTPass* createRTTPass(RTTPassSettings& ci) override;
        void destroyRTTPass(RTTPass* pass) override;

        void reloadShaders() override;
        void drawDebugMenus();

        R2::VK::Core* getCore();
        RenderMeshManager* getMeshManager();
        R2::BindlessTextureManager* getBindlessTextureManager();
        VKTextureManager* getTextureManager();
        const DebugLine* getCurrentDebugLines(size_t* count);
    };
}
