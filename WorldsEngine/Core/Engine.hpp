#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#include <SDL_events.h>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>

#include "AssetDB.hpp"
#include <Render/Camera.hpp>
#include <Util/UniquePtr.hpp>

#ifdef CHECK_NEW_DELETE
void* operator new(size_t count);
void operator delete(void* ptr) noexcept;
#endif

namespace worlds
{
    extern glm::ivec2 windowSize;
    class Renderer;
    class AudioSystem;
    class InputManager;
    class Editor;
    class DotNetScriptEngine;
    class OpenXRInterface;
    class RTTPass;
    class Console;
    class IGameEventHandler;
    class ISystem;
    class Window;
    class PhysicsSystem;

    struct SceneInfo
    {
        std::string name;
        AssetID id;
    };

    struct EngineInitOptions
    {
        EngineInitOptions()
            : useEventThread(false), workerThreadOverride(-1), runAsEditor(false), enableVR(false),
              dedicatedServer(false), eventHandler(nullptr), gameName("Untitled")
        {
        }
        bool useEventThread;
        int workerThreadOverride;
        bool runAsEditor;
        bool enableVR;
        bool dedicatedServer;
        IGameEventHandler* eventHandler;
        const char* gameName;
    };

    struct SceneSettings
    {
        AssetID skybox;
        float skyboxBoost;
    };

    struct PrefabInstanceComponent
    {
        AssetID prefab;
    };

    class EngineArguments
    {
      public:
        static void parseArguments(int argc, char** argv);
        static void addArgument(const char* arg, const char* value = nullptr);
        static bool hasArgument(const char* arg);
        static std::string_view argumentValue(const char* arg);
    };

    class WorldsEngine;
    struct EngineInterfaces
    {
        WorldsEngine* engine;
        OpenXRInterface* vrInterface;
        Renderer* renderer;
        Camera* mainCamera;
        InputManager* inputManager;
        DotNetScriptEngine* scriptEngine;
        PhysicsSystem* physics;
        Editor* editor;
    };

    class SimulationLoop
    {
    public:
        SimulationLoop(const EngineInterfaces& interfaces, IGameEventHandler* evtHandler,
                       entt::registry& registry);
        // returns true if the simulation actually ran
        bool updateSimulation(float& interpAlpha, double timeScale, double deltaTime,
                              bool physicsOnly);
    private:
        void doSimStep(float deltaTime, bool physicsOnly);
        double simAccumulator;
        PhysicsSystem* physics;
        DotNetScriptEngine* scriptEngine;
        entt::registry& registry;
        IGameEventHandler* evtHandler;
    };

    class WorldsEngine
    {
    public:
        WorldsEngine(EngineInitOptions initOptions, char* argv0);
        ~WorldsEngine();

        void run();
        void loadScene(AssetID scene);
        Window& getMainWindow() const
        {
            return *window;
        }
        void quit()
        {
            running = false;
        }
        bool pauseSim;
        bool runAsEditor;
        void destroyNextFrame(entt::entity ent)
        {
            this->nextFrameKillList.push_back(ent);
        }
        [[nodiscard]] double getGameTime() const
        {
            return gameTime;
        }

      private:
        struct InterFrameInfo
        {
            uint64_t lastPerfCounter;
            double deltaTime;
            double lastUpdateTime;
            int frameCounter;
            double lastTickRendererTime;
        };

        static int eventFilter(void* enginePtr, SDL_Event* evt);
        static int windowThread(void* data);
        void setupSDL();
        Window* createWindow();
        void setupPhysfs(char* argv0);
        void tickRenderer(float deltaTime, bool renderImgui = false);
        void runSingleFrame(bool processEvents);

        Window* window;
        int windowWidth, windowHeight;

        bool running;
        bool headless;
        int workerThreadOverride;
        bool enableVR;
        entt::registry registry;

        IGameEventHandler* evtHandler;
        RTTPass* screenRTTPass;
        Camera cam;

        bool sceneLoadQueued = false;
        AssetID queuedSceneID;

        double timeScale = 1.0;
        double gameTime = 0.0;

        EngineInterfaces interfaces;
        UniquePtr<Renderer> renderer;
        UniquePtr<InputManager> inputManager;
        UniquePtr<AudioSystem> audioSystem;
        UniquePtr<Console> console;
#ifdef BUILD_EDITOR
        UniquePtr<Editor> editor;
#endif
        UniquePtr<DotNetScriptEngine> scriptEngine;
        UniquePtr<OpenXRInterface> vrInterface;
        UniquePtr<PhysicsSystem> physicsSystem;
        UniquePtr<SimulationLoop> simLoop;

        std::vector<entt::entity> nextFrameKillList;

        InterFrameInfo interFrameInfo;
    };
}
