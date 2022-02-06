#pragma once
#include <glm/glm.hpp>
#include <entt/entity/fwd.hpp>
#include <SDL.h>

//#include <ImGui/imgui.h>
#include <VR/IVRInterface.hpp>
#include <Core/WorldComponents.hpp>
#include "Camera.hpp"
#include "PackedMaterial.hpp"

typedef void* ImTextureID;

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
        // color in linear space
        glm::vec3 color;
        /*
                              light type
                      shadowmap index  |
                 currently unused   |  |
                                |   |  |
         /-----------------------\/--\/-\
         00000000000000000000000000000000 */
        uint32_t packedFlags;

        // xyz: forward direction or first tube point
        // w: sphere/tube radius or spotlight cutoff
        glm::vec4 packedVars;
        glm::vec3 position; // light position or second tube point
        float distanceCutoff; // distance after which the light isn't visible

        void setLightType(LightType type) {
            packedFlags &= 0b111;
            packedFlags |= (uint32_t)type;
        }

        void setShadowmapIndex(uint32_t shadowmapIdx) {
            packedFlags &= ~(0b1111 << 3);
            packedFlags |= (shadowmapIdx & 0b1111) << 3;
        }
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
            , enableVr(false)
            , enableBloom(true)
            , enableCascadeShadows(true)
            , enableSpotlightShadows(true) {}

        GraphicsSettings(int msaaLevel, int shadowmapRes, bool enableVr)
            : msaaLevel(msaaLevel)
            , shadowmapRes(shadowmapRes)
            , spotShadowmapRes(512)
            , enableVr(enableVr)
            , enableBloom(true)
            , enableCascadeShadows(true)
            , enableSpotlightShadows(true) {
        }

        int msaaLevel;
        int shadowmapRes;
        int spotShadowmapRes;
        bool enableVr;
        bool enableBloom;
        bool enableCascadeShadows;
        bool enableSpotlightShadows;
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

    struct RendererInitInfo {
        SDL_Window* window;
        std::vector<std::string> additionalInstanceExtensions;
        std::vector<std::string> additionalDeviceExtensions;
        bool enableVR;
        VrApi activeVrApi;
        IVRInterface* vrInterface;
        const char* applicationName = nullptr;
    };

    struct RTTPassCreateInfo {
        Camera* cam = nullptr;
        uint32_t width, height;
        float resScale = 1.0f;
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
        float resScale = 1.0f;

        uint32_t actualWidth() { return (uint32_t)(width * resScale); }
        uint32_t actualHeight() { return (uint32_t)(height * resScale); }

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

    class IUITextureManager {
    public:
        virtual ImTextureID loadOrGet(AssetID id) = 0;
        virtual void unload(AssetID id) = 0;
        virtual ~IUITextureManager() {}
    };

    class Renderer {
    public:
        virtual void recreateSwapchain(int newWidth = -1, int newHeight = -1) = 0;
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

        virtual void setImGuiDrawData(void* drawData) = 0;

        virtual RTTPass* createRTTPass(RTTPassCreateInfo& ci) = 0;
        virtual void destroyRTTPass(RTTPass* pass) = 0;

        virtual void triggerRenderdocCapture() = 0;
        virtual void startRdocCapture() = 0;
        virtual void endRdocCapture() = 0;

        virtual IUITextureManager& uiTextureManager() = 0;

        virtual ~Renderer() {}
    };
}
