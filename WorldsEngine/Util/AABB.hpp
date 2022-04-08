#pragma once
#include <glm/vec3.hpp>

namespace worlds {
    struct AABB {
        AABB() = default;
        AABB(glm::vec3 min, glm::vec3 max) 
            : min(min)
            , max(max) {}

        glm::vec3 min;
        glm::vec3 max;

        bool containsPoint(glm::vec3 point) {
            return point.x > min.x && point.x < max.x
                && point.y > min.y && point.y < max.y
                && point.z > min.z && point.z < max.z;
        }

        glm::vec3 center() {
            return (min + max) * 0.5f;
        }

        glm::vec3 extents() {
            return max - min;
        }
    };
}