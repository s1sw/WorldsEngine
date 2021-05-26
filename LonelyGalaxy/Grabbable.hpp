#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <entt/entity/fwd.hpp>
#include <vector>

namespace lg {
    enum class GripType {
        Manual,
        Box,
        Cylinder
    };

    struct Grip {
        GripType gripType;
        union {
            struct {
                glm::vec3 position;
                glm::quat rotation;
            } manual;

            struct {
                glm::vec3 position;
                glm::quat rotation;
                glm::vec3 halfExtents;
            } box;

            struct {
                glm::vec3 position;
                glm::quat rotation;
                float height;
                float radius;
            } cylinder;
        };
    };

    struct Grabbable {
        std::function<void(entt::entity)> onTriggerPressed;
        std::vector<Grip> grips;
    };
}
