#pragma once
#include <Render/Render.hpp>
#include <Render/DebugLines.hpp>
#include <tracy/TracyVulkan.hpp>
#include <robin_hood.h>
#include <R2/R2.hpp>

struct ImDrawData;

namespace R2::VK {
    class Swapchain;
}

namespace worlds {
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

    struct GPUCubemap {
        glm::vec3 extent;
        uint32_t texture;
        glm::vec3 position;
        uint32_t flags;
    };

    struct StandardPushConstants {
        uint32_t modelMatrixIdx;
        uint32_t materialIdx;
        uint32_t vpIdx;
        uint32_t objectId;

        uint32_t pad0;
        uint32_t cubemapIdx2;
        float cubemapBoost;
        uint32_t skinningOffset;

        glm::vec4 texScaleOffset;

        glm::ivec3 screenSpacePickPos;
        uint32_t cubemapIdx;
    };

    struct LightUB {
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

    struct ModelMatrices {
        static const uint32_t SIZE = 4096;
        glm::mat4 modelMatrices[SIZE];
    };

    struct ShadowCascadeInfo {
        bool shadowCascadeNeeded = false;
        glm::mat4 matrices[4];
        float texelsPerUnit[4];
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

    class PipelineCacheSerializer {
    public:
        static void loadPipelineCache(const VkPhysicalDeviceProperties&, VkPipelineCacheCreateInfo&);
        static void savePipelineCache(const VkPhysicalDeviceProperties&, const VkPipelineCache&, const VkDevice&);
    };

    class VKUITextureManager : public IUITextureManager {
    public:
        VKUITextureManager(VKRenderer* renderer);
        ImTextureID loadOrGet(AssetID id) override;
        void unload(AssetID id) override;
        ~VKUITextureManager();
    private:
        struct UITexInfo {
            VkDescriptorSet ds;
        };

        UITexInfo* load(AssetID id);
        VKRenderer* renderer;
        robin_hood::unordered_map<AssetID, UITexInfo*> texInfo;
    };

    class VKRenderer : public Renderer {
        R2::VK::Core* core;
        R2::VK::Swapchain* swapchain;
    public:
        VKRenderer(const RendererInitInfo& initInfo, bool* success);

        void frame() override;

        void preloadMesh(AssetID id) override;
        void unloadUnusedAssets(entt::registry& reg) override;
        void reloadContent(ReloadFlags flags) override;

        float getLastRenderTime() const override;
        void setVRPredictAmount(float amt) override;

        void setVsync(bool vsync) override;
        bool getVsync() const override;

        const RenderDebugStats& getDebugStats() const override;
        void uploadSceneAssets(entt::registry& reg) override;

        void setImGuiDrawData(void* drawData) override;

        RTTPass* createRTTPass(RTTPassCreateInfo& ci) override;
        void destroyRTTPass(RTTPass* pass) override;

        void triggerRenderdocCapture() override;
        void startRdocCapture() override;
        void endRdocCapture() override;

        IUITextureManager& uiTextureManager() override;

        ~VKRenderer();
    };

    enum class ShaderVariantFlags : uint32_t {
        None = 0,
        AlphaTest = 1,
        Skinnning = 2,
        NoBackfaceCulling = 4
    };

    inline ShaderVariantFlags operator|(ShaderVariantFlags a, ShaderVariantFlags b) {
        return static_cast<ShaderVariantFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline ShaderVariantFlags operator|=(ShaderVariantFlags& a, ShaderVariantFlags b) {
        return (a = a | b);
    }

    struct PipelineKey {
        bool enablePicking;
        int msaaSamples;
        ShaderVariantFlags flags;
        AssetID overrideFS = INVALID_ASSET;

        uint32_t hash();
    };
}
