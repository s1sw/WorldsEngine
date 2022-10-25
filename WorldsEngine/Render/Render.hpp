#pragma once
#include <SDL_video.h>
#include <entt/entity/lw_fwd.hpp>
#include <glm/glm.hpp>
#include <string>

#include "Camera.hpp"
#include "PackedMaterial.hpp"
#include <Render/PickParams.hpp>
#include <Core/WorldComponents.hpp>

typedef void* ImTextureID;

namespace worlds
{
    struct EngineInterfaces;
    const int NUM_SHADOW_LIGHTS = 16;
#pragma pack(push, 1)
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        float bitangentSign;
        glm::vec2 uv;
        glm::vec2 uv2;
    };
#pragma pack(pop)

    struct MultiVP
    {
        glm::mat4 views[2];
        glm::mat4 projections[2];
        glm::mat4 inverseVP[2];
        glm::vec4 viewPos[2];
        int screenWidth;
        int screenHeight;
        int pad0;
        int pad1;
    };

    struct PackedLight
    {
        // color in linear space
        glm::vec3 color;
        /*
                              light type
                      shadowmap index  |
            spotlight outer cutoff  |  |
         currently unused       |   |  |
                        |       |   |  |
         /---------------\/------\/--\/-\
         00000000000000000000000000000000 */
        uint32_t packedFlags;

        // xyz: forward direction or first tube point
        // w: sphere/tube radius or spotlight cutoff
        glm::vec3 direction;
        float spotCutoff;
        glm::vec3 position;   // light position or second tube point
        float distanceCutoff; // distance after which the light isn't visible

        void setLightType(LightType type)
        {
            packedFlags &= ~0b111;
            packedFlags |= (uint32_t)type;
        }

        void setShadowmapIndex(uint32_t shadowmapIdx)
        {
            packedFlags &= ~(0b1111 << 3);
            packedFlags |= (shadowmapIdx & 0b1111) << 3;
        }

        void setOuterCutoff(float outerCutoff)
        {
            uint8_t quantized = (uint8_t)(glm::clamp(glm::cos(outerCutoff), 0.0f, 1.0f) * 255);
            packedFlags &= ~(0xFF << 7);
            packedFlags |= (uint32_t)(quantized & 0xFF) << 7;
        }
    };

    struct RenderDebugStats
    {
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
        double lightCullTime;
    };

    enum class VKVendor
    {
        AMD,
        Nvidia,
        Intel,
        Qualcomm,
        Other
    };

    struct RendererInitInfo
    {
        SDL_Window* window;
        std::vector<std::string> additionalInstanceExtensions;
        std::vector<std::string> additionalDeviceExtensions;
        bool enableVR;
        const char* applicationName = nullptr;
        const EngineInterfaces& interfaces;
    };

    struct RTTPassSettings
    {
        Camera* cam = nullptr;
        uint32_t width, height;
        bool useForPicking;
        bool enableShadows;
        bool staticsOnly = false;
        int msaaLevel = 0;
        int numViews = 1;
        entt::registry* registryOverride = nullptr;
        bool renderDebugShapes = true;
        bool outputToXR = false;
        bool setViewsFromXR = false;
        bool enableHDRCapture = false;
    };

    /**
     * A complete rendering pass, either to the screen or an accessible texture.
     */
    class RTTPass
    {
    public:
        //! Controls the order in which the RTTPasses are executed.
        int drawSortKey = 0;
        uint32_t width, height;

        //! Whether the RTTPass should be rendered every frame.
        bool active = false;

        virtual void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix) = 0;

        //! Get a float array of the HDR pass result.
        virtual void requestHDRData() = 0;
        virtual bool getHDRData(float*& out) = 0;
        virtual void resize(int newWidth, int newHeight) = 0;
        virtual ImTextureID getUITextureID() = 0;

    protected:
        virtual ~RTTPass()
        {}

        friend class Renderer;
    };

    enum class ReloadFlags
    {
        Textures = 1,
        Materials = 2,
        Cubemaps = 4,
        Meshes = 8,
        All = 15
    };

    inline ReloadFlags operator|(ReloadFlags l, ReloadFlags r)
    {
        return (ReloadFlags)((uint32_t)l | (uint32_t)r);
    }

    /**
     * Loads and unloads textures to be used in Dear ImGui.
     */
    class IUITextureManager
    {
    public:
        virtual ImTextureID loadOrGet(AssetID id) = 0;
        virtual void unload(AssetID id) = 0;
        virtual ~IUITextureManager()
        {
        }
    };

    /**
     * Base renderer class that doesn't expose any API-specific details.
     */
    class Renderer
    {
    public:
        virtual void frame(entt::registry& reg, float deltaTime) = 0;

        //! Gets time spent rendering the scene on the GPU.
        virtual float getLastGPUTime() const = 0;
        virtual void setVRUsedPose(glm::mat4x4 pose) = 0;

        virtual void setVsync(bool vsync) = 0;
        virtual bool getVsync() const = 0;

        virtual RenderDebugStats& getDebugStats() = 0;
        virtual IUITextureManager* getUITextureManager() = 0;

        virtual void setImGuiDrawData(void* drawData) = 0;

        virtual RTTPass* createRTTPass(RTTPassSettings& ci) = 0;
        virtual void destroyRTTPass(RTTPass* pass) = 0;

        virtual void requestPick(PickParams params) = 0;
        virtual bool getPickResult(uint32_t& entityId) = 0;

        virtual void reloadShaders() = 0;

        virtual ~Renderer()
        {
        }
    };
}
