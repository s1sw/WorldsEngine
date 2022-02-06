#pragma once
#include "Physics/Physics.hpp"
#define NOMINMAX
#include <Core/IGameEventHandler.hpp>
#include <Core/Console.hpp>
#include <Core/Engine.hpp>
#include <deque>

namespace lg {
    class EventHandler : public worlds::IGameEventHandler {
    public:
        EventHandler(bool dedicatedServer);
        void init(entt::registry& registry, worlds::EngineInterfaces interfaces) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void simulate(entt::registry& registry, float simStep) override;
        void onSceneStart(entt::registry& reg) override;
        void shutdown(entt::registry& registry) override;
    private:
        worlds::EngineInterfaces interfaces;
        worlds::IVRInterface* vrInterface;
        worlds::Renderer* renderer;
        worlds::InputManager* inputManager;
        worlds::Camera* camera;
        worlds::WorldsEngine* engine;
        entt::registry* reg;
    };
}
