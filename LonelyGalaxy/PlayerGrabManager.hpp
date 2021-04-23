#pragma once
#include "LocospherePlayerSystem.hpp"
#include <VR/IVRInterface.hpp>

namespace lg {
    class PlayerGrabManager {
    public:
        PlayerGrabManager(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void simulate(float simStep);
        void setPlayerEntity(entt::entity playerEnt);
    private:
        void updateHandGrab(PlayerRig& rig, entt::entity ent, float deltaTime);
        entt::entity playerEnt;
        entt::registry& registry;
        worlds::InputActionHandle lGrab;
        worlds::InputActionHandle rGrab;
        worlds::EngineInterfaces interfaces;
    };
}
