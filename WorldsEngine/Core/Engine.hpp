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
    const int NUM_SUBMESH_MATS = 32;
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

    class ConVar {
    public:
        ConVar(const char* name, const char* defaultValue, const char* help = nullptr);
        ~ConVar();
        float getFloat() const { return parsedFloat; }
        int getInt() const { return parsedInt; }
        const char* getString() const { return value.c_str(); }
        const char* getName() const { return name; }
        const char* getHelp() const { return help; }
        void setValue(std::string newValue);
        operator float() const { return getFloat(); }
        operator bool() const { return (bool)getInt(); }
    private:
        const char* help;
        const char* name;
        std::string value;
        int parsedInt;
        float parsedFloat;

        friend class Console;
    };

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
        std::vector<const char*> cmdLineOptions;
    };

    struct SceneSettings {
        AssetID skybox;
        float skyboxBoost;
    };

    struct PrefabInstanceComponent {
        AssetID prefab;
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
        const SceneInfo& getCurrentSceneInfo() const { return currentScene; }
        void quit() { running = false; }
        bool pauseSim;
        bool runAsEditor;
        void destroyNextFrame(entt::entity ent) { this->nextFrameKillList.push_back(ent); }
        [[nodiscard]] double getGameTime() const { return gameTime; }
        bool hasCommandLineArg(const char* arg);
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

        SceneInfo currentScene;
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

        std::vector<const char*> cmdLineOptions;
    };

    enum class StaticFlags : uint8_t {
        None = 0,
        Audio = 1,
        Rendering = 2,
        Navigation = 4
    };

    enum class UVOverride {
        None,
        XY,
        XZ,
        ZY,
        PickBest
    };

    struct WorldObject {
        WorldObject(AssetID material, AssetID mesh)
            : staticFlags(StaticFlags::None)
            , mesh(mesh)
            , texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f)
            , uvOverride(UVOverride::None) {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                materials[i] = material;
                presentMaterials[i] = false;
            }
            presentMaterials[0] = true;
        }

        StaticFlags staticFlags;
        AssetID materials[NUM_SUBMESH_MATS];
        slib::Bitset<NUM_SUBMESH_MATS> presentMaterials;
        AssetID mesh;
        glm::vec4 texScaleOffset;
        UVOverride uvOverride;
    };

    struct Bone {
        glm::mat4 restPose;
        uint32_t id;
    };

    class Skeleton {
    public:
        std::vector<Bone> bones;
    };

    class Pose {
    public:
        std::vector<glm::mat4> boneTransforms;
    };

    struct SkinnedWorldObject : public WorldObject {
        SkinnedWorldObject(AssetID material, AssetID mesh)
            : WorldObject(material, mesh) {
            currentPose.boneTransforms.resize(64); // TODO

            for (glm::mat4& t : currentPose.boneTransforms) {
                t = glm::mat4{1.0f};
            }
        }
        Pose currentPose;
    };

    struct UseWireframe {};

    enum class LightType {
        Point,
        Spot,
        Directional,
        Sphere,
        Tube
    };

    struct WorldLight {
        WorldLight() {}
        WorldLight(LightType type) : type(type) {}

        // Whether the light should be actually rendered
        bool enabled = true;
        LightType type = LightType::Point;
        glm::vec3 color = glm::vec3{1.0f};
        float intensity = 1.0f;

        // Angle of the spotlight cutoff in radians
        float spotCutoff = glm::pi<float>() * 0.5f;

        // Physical dimensions of a tube light
        float tubeLength = 0.25f;
        float tubeRadius = 0.1f;

        // Shadowing settings
        bool enableShadows = false;
        uint32_t shadowmapIdx = ~0u;
        float shadowNear = 0.05f;
        float shadowFar = 100.0f;

        float maxDistance = 1.0f;
        // Index of the light in the light buffer
        uint32_t lightIdx = 0u;
    };

    struct EditorLabel {
        std::string label;
    };

    struct DontSerialize {};
    struct HideFromEditor {};
}
