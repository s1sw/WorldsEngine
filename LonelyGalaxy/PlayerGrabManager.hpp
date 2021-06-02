#pragma once
#include "Grabbable.hpp"
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
        float calculateGripScore(Grip& grip, const Transform& handTransform, const Transform& grabbingTransform);
        void handleGrab(entt::entity grabbing, entt::entity hand);
        entt::entity playerEnt;
        entt::registry& registry;
        worlds::InputActionHandle lGrab;
        worlds::InputActionHandle rGrab;
        worlds::InputActionHandle lTrigger;
        worlds::InputActionHandle rTrigger;
        worlds::EngineInterfaces interfaces;
    };
}
