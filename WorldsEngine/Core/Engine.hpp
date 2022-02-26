#pragma once
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include <slib/Bitset.hpp>

#include <SDL_events.h>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>

#include "AssetDB.hpp"
#include <Render/Camera.hpp>

#ifdef CHECK_NEW_DELETE
void* operator new(size_t count);
void operator delete(void* ptr) noexcept;
#endif

namespace worlds {
    extern glm::ivec2 windowSize;
    class Renderer;
    class AudioSystem;
    class InputManager;
    class Editor;
    class DotNetScriptEngine;
    class OpenVRInterface;
    class RTTPass;
    class Console;
    class IGameEventHandler;
    class ISystem;
    class Window;

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
            , eventHandler(nullptr)
            , gameName("Untitled") {}
        bool useEventThread;
        int workerThreadOverride;
        bool runAsEditor;
        bool enableVR;
        bool dedicatedServer;
        IGameEventHandler* eventHandler;
        const char* gameName;
    };

    struct SceneSettings {
        AssetID skybox;
        float skyboxBoost;
    };

    struct PrefabInstanceComponent {
        AssetID prefab;
    };

    class EngineArguments {
    public:
        static void parseArguments(int argc, char** argv);
        static void addArgument(const char* arg, const char* value = nullptr);
        static bool hasArgument(const char* arg);
        static std::string_view argumentValue(const char* arg);
    };

    class WorldsEngine {
    public:
        WorldsEngine(EngineInitOptions initOptions, char* argv0);
        ~WorldsEngine();

        void mainLoop();
        void loadScene(AssetID scene);
        void createStartupScene();
        void addSystem(ISystem* system);
        Window& getMainWindow() const { return *window; }
        void quit() { running = false; }
        bool pauseSim;
        bool runAsEditor;
        void destroyNextFrame(entt::entity ent) { this->nextFrameKillList.push_back(ent); }
        [[nodiscard]] double getGameTime() const { return gameTime; }
        [[deprecated("Use EngineArguments")]] bool hasCommandLineArg(const char* arg);
    private:
        struct DebugTimeInfo {
            double deltaTime;
            double updateTime;
            double simTime;
            double lastUpdateTime;
            int frameCounter;
        };

        static int eventFilter(void* enginePtr, SDL_Event* evt);
        static int windowThread(void* data);
        static int renderThread(void* data);
        void setupSDL();
        Window* createWindow();
        void setupPhysfs(char* argv0);
        void drawDebugInfoWindow(DebugTimeInfo timeInfo);
        void updateSimulation(float& interpAlpha, double deltaTime);
        void doSimStep(float deltaTime);
        void tickRenderer(bool renderImgui = false);

        Window* window;
        int windowWidth, windowHeight;

        bool running;
        bool dedicatedServer;
        entt::registry registry;

        IGameEventHandler* evtHandler;
        RTTPass* screenRTTPass;
        Camera cam;

        bool sceneLoadQueued = false;
        AssetID queuedSceneID;

        double timeScale = 1.0;
        double gameTime = 0.0;
        double simAccumulator;

        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<InputManager> inputManager;
        std::unique_ptr<AudioSystem> audioSystem;
        std::unique_ptr<Console> console;
        std::unique_ptr<Editor> editor;
        std::unique_ptr<DotNetScriptEngine> scriptEngine;
        std::unique_ptr<OpenVRInterface> openvrInterface;

        std::vector<ISystem*> systems;
        std::vector<entt::entity> nextFrameKillList;
    };
}
