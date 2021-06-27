#pragma once
#include "Core/Transform.hpp"
#include "PidController.hpp"
#include <entt/entity/registry.hpp>
#include <Core/ISystem.hpp>

namespace lg {
    struct DroneAI {
        StablePD pd;
        V3PidController rotationPID;

        glm::vec3 maxPositionalForces{1000.0f};
        glm::vec3 minPositionalForces{-1000.0f};
        glm::vec3 currentTarget;

        Transform firePoint;

        float timeSinceLastShot = 0.0f;
        float timeSinceLastBurst = 0.0f;
        bool firingBurst = false;
    };

    class DroneAISystem : public worlds::ISystem {
    public:
        DroneAISystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void onSceneStart(entt::registry& registry) override;
        void simulate(entt::registry& registry, float simStep) override;
    };
}
