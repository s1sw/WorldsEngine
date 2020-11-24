#pragma once
#include <entt/entt.hpp>
#include "IGameEventHandler.hpp"

namespace worlds {
    class ISystem {
    public:
        virtual void preSimUpdate(entt::registry& registry, float deltaTime) {};
        virtual void update(entt::registry& registry, float deltaTime, float interpAlpha) {};
        virtual void simulate(entt::registry& registry, float stepTime) {};
        virtual void onSceneStart(entt::registry& registry) {};
        virtual void shutdown(entt::registry& registry) {};
        virtual ~ISystem() {}
    };
}
