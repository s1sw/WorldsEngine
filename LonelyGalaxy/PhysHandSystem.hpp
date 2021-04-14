#pragma once
#include <entt/entt.hpp>
#include <Physics/PhysicsActor.hpp>
#include "PidController.hpp"
#include <Core/Transform.hpp>
#include <Physics/Physics.hpp>
#include <Core/ISystem.hpp>

namespace lg {
    enum class FollowHand {
        None,
        LeftHand,
        RightHand
    };

    /**
     * Component for a PID-driven physics hand. Attempts to follow targetWorldPos and targetWorldRot.
     */
    struct PhysHand {
        glm::vec3 targetWorldPos;
        glm::quat targetWorldRot;
        V3PidController posController;
        V3PidController rotController;

        /**
         * VR hand to follow. If set, targetWorldPos and targetWorldRot
         * will be automatically set from the hand's transform.
         */
        FollowHand follow;

        entt::entity locosphere = entt::null; ///< Locosphere entity used for velocity compensation.

        // Inertia tensor override for compensating for unbalanced objects.
        bool useOverrideIT = false;
        physx::PxMat33 overrideIT;

        float forceLimit = 1000.0f; ///< Force limit in Newtons
        float torqueLimit = 15.0f; ///< Torque limit in Newton-metres
        float forceMultiplier = 1.0f;

        // Grab heading info
        entt::entity goingTo = entt::null;
        FollowHand oldFollowHand = FollowHand::None;
    };

    class PhysHandSystem : public worlds::ISystem {
    public:
        PhysHandSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void simulate(entt::registry& registry, float simStep) override;
    private:
        void setTargets(PhysHand& hand, entt::entity ent, float deltaTime);
        worlds::EngineInterfaces interfaces;
        entt::registry& registry;
    };
}
