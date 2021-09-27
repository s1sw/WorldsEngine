#pragma once
#include <glm/glm.hpp>
#include "../VR/IVRInterface.hpp"
#include "Core/Engine.hpp"
#include "Camera.hpp"
#include <SDL.h>
#include "PackedMaterial.hpp"

namespace worlds {
    const int NUM_SHADOW_LIGHTS = 4;
#pragma pack(push, 1)
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        float bitangentSign;
        glm::vec2 uv;
        glm::vec2 uv2;
    };
#pragma pack(pop)

    struct WorldCubemap {
        AssetID cubemapId;
        glm::vec3 extent{0.0f};
        bool cubeParallax = false;
        int priority = 0;
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
        glm::vec3 pack2;
        uint32_t shadowIdx;
        float distanceCutoff;
        uint32_t pad0;
        uint32_t pad1;
        uint32_t pad2;
    };

    struct ProxyAOComponent {
        glm::vec3 bounds;
    };

    struct SphereAOProxy {
        float radius;
    };

    struct GraphicsSettings {
        GraphicsSettings()
            : msaaLevel(2)
            , shadowmapRes(1024)
            , spotShadowmapRes(512)
            , enableVr(false) {}
        GraphicsSettings(int msaaLevel, int shadowmapRes, bool enableVr)
            : msaaLevel(msaaLevel)
            , shadowmapRes(shadowmapRes)
            , spotShadowmapRes(512)
            , enableVr(enableVr) {
        }

        int msaaLevel;
        int shadowmapRes;
        int spotShadowmapRes;
        bool enableVr;
    };

    struct SubmeshInfo {
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    struct RenderDebugStats {
        int numDrawCalls;
        int numCulledObjs;
        uint64_t vramUsage;
        int numRTTPasses;
        int numActiveRTTPasses;
        int numPipelineSwitches;
        int numTriangles;
        int numLightsInView;
        double imgAcquisitionTime;
        double cmdBufWriteTime;
        double cmdBufFenceWaitTime;
        double imgFenceWaitTime;
    };

    enum class VKVendor {
        AMD,
        Nvidia,
        Intel,
        Other
    };

    struct PassSettings {
        bool enableVR;
        bool enableShadows;
        int msaaSamples;
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

    struct RTTPassCreateInfo {
        Camera* cam = nullptr;
        uint32_t width, height;
        bool isVr;
        bool useForPicking;
        bool enableShadows;
        bool outputToScreen;
        int msaaLevel = 0;
    };

    class RTTPass {
    public:
        int drawSortKey = 0;
        uint32_t width, height;
        bool isValid = true;
        bool active = false;

        virtual void drawNow(entt::registry& world) = 0;
        virtual void requestPick(int x, int y) = 0;
        virtual bool getPickResult(uint32_t* result) = 0;
        virtual float* getHDRData() = 0;
        virtual ~RTTPass() {}
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

    class Renderer {
    public:
        virtual void recreateSwapchain() = 0;
        virtual void frame(Camera& cam, entt::registry& reg) = 0;
        virtual void preloadMesh(AssetID id) = 0;
        virtual void unloadUnusedMaterials(entt::registry& reg) = 0;
        virtual void reloadContent(ReloadFlags flags) = 0;
        virtual float getLastRenderTime() const = 0;
        virtual void setVRPredictAmount(float amt) = 0;
        virtual void setVsync(bool vsync) = 0;
        virtual bool getVsync() const = 0;
        virtual const RenderDebugStats& getDebugStats() const = 0;
        virtual void uploadSceneAssets(entt::registry& reg) = 0;

        virtual RTTPass* createRTTPass(RTTPassCreateInfo& ci) = 0;
        virtual void destroyRTTPass(RTTPass* pass) = 0;

        virtual void triggerRenderdocCapture() = 0;
        virtual void startRdocCapture() = 0;
        virtual void endRdocCapture() = 0;

        virtual ~Renderer() {}
    };
}
