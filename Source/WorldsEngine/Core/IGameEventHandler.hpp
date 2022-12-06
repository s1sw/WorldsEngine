#pragma once
#include <Core/Engine.hpp>
#include <entt/entity/fwd.hpp>

namespace worlds
{
    class IGameEventHandler
    {
      public:
        virtual void init(entt::registry& registry, EngineInterfaces interfaces) {}
        virtual void preSimUpdate(entt::registry& registry, float deltaTime) {}
        virtual void update(entt::registry& registry, float deltaTime, float interpAlpha) {}
        virtual void simulate(entt::registry& registry, float stepTime) {}
        virtual void onSceneStart(entt::registry& registry) {}
        virtual void shutdown(entt::registry& registry) {}
        virtual ~IGameEventHandler() {}
    };
}
