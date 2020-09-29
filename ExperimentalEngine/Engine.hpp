#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "AssetDB.hpp"
#include "IGameEventHandler.hpp"
#include "JobSystem.hpp"
#include <bitset>

#define NUM_SUBMESH_MATS 32
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
        bool runAsEditor;
        bool enableVR;
        IGameEventHandler* eventHandler;
    };

    void initEngine(EngineInitOptions initOptions, char* argv0);

    struct WorldObject {
        WorldObject(AssetID material, AssetID mesh)
            : mesh(mesh)
            , texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f) {
            materials[0] = material;
            for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                presentMaterials[i] = false;
                materialIdx[i] = ~0u;
            }

            presentMaterials[0] = true;
        }

        AssetID materials[NUM_SUBMESH_MATS];
        std::bitset<NUM_SUBMESH_MATS> presentMaterials;
        AssetID mesh;
        glm::vec4 texScaleOffset;
        uint32_t materialIdx[NUM_SUBMESH_MATS];
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