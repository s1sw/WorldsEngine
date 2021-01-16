#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "AssetDB.hpp"
#include "IGameEventHandler.hpp"
#include "JobSystem.hpp"
#include <bitset>
#include "OpenVRInterface.hpp"
#include "ISystem.hpp"
#include "Camera.hpp"
#include "Console.hpp"
#include "LuaVM.hpp"

#define NUM_SUBMESH_MATS 32
namespace worlds {
    extern glm::ivec2 windowSize;
    extern JobSystem* g_jobSys;
    class VKRenderer;
    class PolyRenderPass;
    class AudioSystem;
    class InputManager;
    class Editor;
    typedef uint32_t RTTPassHandle;

    struct SceneInfo {  
        std::string name;
        AssetID id;
    };

    struct EngineInitOptions {
        EngineInitOptions()
            : useEventThread(false)
            , workerThreadOverride(-1)
            , runAsEditor(false)
            , enableVR(false)
            , dedicatedServer(false) 
            , eventHandler(nullptr) {}
        bool useEventThread;
        int workerThreadOverride;
        bool runAsEditor;
        bool enableVR;
        bool dedicatedServer;
        IGameEventHandler* eventHandler;
    };

    class WorldsEngine {
    public:
        WorldsEngine(EngineInitOptions initOptions, char* argv0);
        void mainLoop();
        ~WorldsEngine();
        void loadScene(AssetID scene);
        void addSystem(ISystem* system);
        bool pauseSim;
        bool runAsEditor;
    private:
        struct DebugTimeInfo {
            double deltaTime;
            double updateTime;
            double simTime;
            double lastUpdateTime;
            int frameCounter;
        };

        static int windowThread(void* data);
        void setupSDL();
        static SDL_Window* createSDLWindow();
        void setupPhysfs(char* argv0);
        void createStartupScene();
        void drawDebugInfoWindow(DebugTimeInfo timeInfo);
        void updateSimulation(float& interpAlpha, double deltaTime);
        bool running;
        double simAccumulator;
        bool dedicatedServer;
        VKRenderer* renderer;
        entt::registry registry;
        IGameEventHandler* evtHandler;
        std::unique_ptr<InputManager> inputManager;
        std::unique_ptr<AudioSystem> audioSystem;
        RTTPassHandle screenRTTPass;
        Camera cam;
        std::unique_ptr<Console> console;
        std::unique_ptr<Editor> editor;
        std::unique_ptr<LuaVM> luaVM;
        OpenVRInterface openvrInterface;
        double timeScale = 1.0;

        std::vector<ISystem*> systems;
    };

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
