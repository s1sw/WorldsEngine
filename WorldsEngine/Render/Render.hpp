#pragma once
#include <glm/glm.hpp>
#include <SDL_video.h>
#include <entt/entity/lw_fwd.hpp>

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
        glm::mat4 inverseVP[2];
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


    /**
     * Graphics settings applied to individual RTT passes.
     */
    struct GraphicsSettings {
        GraphicsSettings()
            : msaaLevel(2)
            , shadowmapRes(1024)
            , enableVr(false) {}

        GraphicsSettings(int msaaLevel, int shadowmapRes, bool enableVr)
            : msaaLevel(msaaLevel)
            , shadowmapRes(shadowmapRes)
            , enableVr(enableVr) {}

        int msaaLevel; //!< MSAA level (1x, 2x, 4x, 8x etc.)
        int shadowmapRes; //!< Shadowmap resolution for cascade shadows
        int spotShadowmapRes = 1024; //!< Shadowmap resolution specifically for spotlights
        bool enableVr;
        bool enableBloom = true;
        bool enableCascadeShadows = true;
        bool enableSpotlightShadows = true;
        bool enableDebugLines = true;
        bool staticsOnly = false;
        float resolutionScale = 1.0f;
    };

    struct SubmeshInfo {
        uint32_t indexOffset; //!< The offset of the submesh in the mesh index buffer.
        uint32_t indexCount; //!< The number of indices in the submesh.
        int materialIndex;
        glm::vec3 aabbMax;
        glm::vec3 aabbMin;
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
        int numTexturesLoaded;
        int numMaterialsLoaded;
        double imgAcquisitionTime;
        double cmdBufWriteTime;
        double cmdBufFenceWaitTime;
        double imgFenceWaitTime;
    };

    enum class VKVendor {
        AMD,
        Nvidia,
        Intel,
        Qualcomm,
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
        bool staticsOnly = false;
        int msaaLevel = 0;
        entt::registry* registryOverride = nullptr;
        bool renderDebugShapes = true;
    };

    /**
     * A complete rendering pass, either to the screen or an accessible texture.
     */
    class RTTPass {
    public:
        //! Controls the order in which the RTTPasses are executed.
        int drawSortKey = 0;
        uint32_t width, height;

        //! Whether the RTTPass should be rendered every frame.
        bool active = false;
        float resScale = 1.0f;

        uint32_t actualWidth() { return (uint32_t)(width * resScale); }
        uint32_t actualHeight() { return (uint32_t)(height * resScale); }

        //! Draws the render pass immediately. Slow!!
        virtual void drawNow(entt::registry& world) = 0;

        //! Requests an entity pick at the specified coordinates.
        virtual void requestPick(int x, int y) = 0;
        //! Gets the result of the last request pick.
        /**
         * \param result The entity ID under the coordinates specified in requestPick.
         * \return True if the pick results were retrieved, false if the results aren't ready yet.
         */
        virtual bool getPickResult(uint32_t* result) = 0;

        //! Get a float array of the HDR pass result.
        virtual float* getHDRData() = 0;
        virtual void resize(int newWidth, int newHeight) = 0;
        virtual void setResolutionScale(float newScale) = 0;
    protected:
        virtual ~RTTPass() {}
        friend class Renderer;
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

    /**
     * Loads and unloads textures to be used in Dear ImGui.
     */
    class IUITextureManager {
    public:
        virtual ImTextureID loadOrGet(AssetID id) = 0;
        virtual void unload(AssetID id) = 0;
        virtual ~IUITextureManager() {}
    };

    /**
     * Base renderer class that doesn't expose any API-specific details.
     */
    class Renderer {
    public:
        virtual void recreateSwapchain(int newWidth = -1, int newHeight = -1) = 0;
        virtual void frame(Camera& cam, entt::registry& reg) = 0;

        //! Gets time spent rendering the scene on the GPU.
        virtual float getLastRenderTime() const = 0;
        //! Sets the prediction amount used on head transforms for VR.
        virtual void setVRPredictAmount(float amt) = 0;

        virtual void setVsync(bool vsync) = 0;
        virtual bool getVsync() const = 0;

        virtual const RenderDebugStats& getDebugStats() const = 0;

        //! Load all materials, textures, models and cubemaps referenced in the scene.
        virtual void uploadSceneAssets(entt::registry& reg) = 0;
        //! Loads a specified mesh.
        virtual void preloadMesh(AssetID id) = 0;
        //! Unloads any materials, textures, models and cubemaps not referenced in the scene.
        //! Reloads the assets specified by the flags.
        virtual void unloadUnusedAssets(entt::registry& reg) = 0;
        virtual void reloadContent(ReloadFlags flags) = 0;

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
