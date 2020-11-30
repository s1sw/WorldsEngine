#pragma once
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "PidController.hpp"
#include <IVRInterface.hpp>
#include <Transform.hpp>
#include <Physics.hpp>
#include <ISystem.hpp>

namespace converge {
    struct PhysHand {
        glm::vec3 targetWorldPos;
        glm::quat targetWorldRot;
        V3PidController posController;
        V3PidController rotController;
    };

    class PhysHandSystem : public worlds::ISystem {
    public:
        PhysHandSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void simulate(entt::registry& registry, float simStep) override;
    private:
        entt::registry& registry;
    };
}
