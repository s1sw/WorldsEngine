#pragma once
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "PidController.hpp"
#include <IVRInterface.hpp>
#include <Transform.hpp>
#include <Physics.hpp>
#include <ISystem.hpp>

namespace converge {
    enum class FollowHand {
        None,
        LeftHand,
        RightHand
    };

    struct PhysHand {
        PhysHand() : locosphere {entt::null} {}
        glm::vec3 targetWorldPos;
        glm::quat targetWorldRot;
        StableHandPD posController;
        V3PidController rotController;
        FollowHand follow;
        entt::entity locosphere;
    };

    class PhysHandSystem : public worlds::ISystem {
    public:
        PhysHandSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void update(entt::registry& registry, float deltaTime, float) override;
        void simulate(entt::registry& registry, float simStep) override;
    private:
        void setTargets(PhysHand& hand, entt::entity ent, float deltaTime);
        worlds::EngineInterfaces interfaces;
        entt::registry& registry;
    };
}
