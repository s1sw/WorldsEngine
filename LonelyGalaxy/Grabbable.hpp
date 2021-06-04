#pragma once
#include "Core/Transform.hpp"
#include "MathsUtil.hpp"
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

    enum class GripHand {
        Left,
        Right,
        Both
    };

    struct Grip {
        GripType gripType = GripType::Manual;
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat();
        GripHand hand = GripHand::Both;
        bool inUse = false;
        bool exclusive = false;

        union {
            struct {
                glm::vec3 halfExtents;
            } box;

            struct {
                float height;
                float radius;
            } cylinder;
        };

        Transform getWorldSpace(const Transform& objectTransform) {
            Transform thisGripTransform{ position, rotation };
            return thisGripTransform.transformBy(objectTransform);
        }

        float calcDistance(glm::vec3 handPos, const Transform& objectTransform) {
            return glm::distance(handPos, getWorldSpace(objectTransform).position);
        }

        float calcRotationAlignment(glm::quat handRot, const Transform& objectTransform) {
            return glm::dot(fixupQuat(handRot), fixupQuat(getWorldSpace(objectTransform).rotation));
        }
    };

    struct Grabbable {
        std::function<void(entt::entity)> onTriggerPressed;
        std::function<void(entt::entity)> onTriggerReleased;
        std::function<void(entt::entity)> onTriggerHeld;
        std::vector<Grip> grips;
    };
}
