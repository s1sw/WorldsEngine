#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "AssetDB.hpp"
#include "IGameEventHandler.hpp"
#include "JobSystem.hpp"

namespace worlds {
    extern glm::ivec2 windowSize;
    extern JobSystem* g_jobSys;
    class VKRenderer;
    class PolyRenderPass;

    struct SceneInfo {
        std::string name;
        AssetID id;
    };

    struct EngineInitOptions {
        bool useEventThread;
        int workerThreadOverride;
        IGameEventHandler* eventHandler;
    };

    void initEngine(EngineInitOptions initOptions, char* argv0);

    struct WorldObject {
        WorldObject(AssetID material, AssetID mesh)
            : material(material)
            , mesh(mesh)
            , texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f)
            , materialIdx(~0u) {
        }

        AssetID material;
        AssetID mesh;
        glm::vec4 texScaleOffset;
        uint32_t materialIdx;
    };

    struct UseWireframe {};

    enum class LightType {
        Point,
        Spot,
        Directional
    };

    struct WorldLight {
        WorldLight() : type(LightType::Point), color(1.0f), spotCutoff(1.35f) {}
        WorldLight(LightType type) : type(type), color(1.0f), spotCutoff(1.35f) {}
        LightType type;
        glm::vec3 color;
        float spotCutoff;
    };
}