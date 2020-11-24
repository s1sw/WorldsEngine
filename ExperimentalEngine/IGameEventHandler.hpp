#pragma once
#include <entt/entt.hpp>

namespace worlds {
    class WorldsEngine;
    class VKRenderer;
    struct Camera;
    class InputManager;
    class IVRInterface;

    struct EngineInterfaces {
        IVRInterface* vrInterface;
        VKRenderer* renderer;
        Camera* mainCamera;
        InputManager* inputManager;
        WorldsEngine* engine;
    };

    class IGameEventHandler {
    public:
        virtual void init(entt::registry& registry, EngineInterfaces interfaces) = 0;
        virtual void preSimUpdate(entt::registry& registry, float deltaTime) = 0;
        virtual void update(entt::registry& registry, float deltaTime, float interpAlpha) = 0;
        virtual void simulate(entt::registry& registry, float stepTime) = 0;
        virtual void onSceneStart(entt::registry& registry) = 0;
        virtual void shutdown(entt::registry& registry) = 0;
        virtual ~IGameEventHandler() {}
    };
}
