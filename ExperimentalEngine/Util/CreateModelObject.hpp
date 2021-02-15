#pragma once
#include <entt/entt.hpp>
#include "../Core/Transform.hpp"
#include "../Core/Engine.hpp"

namespace worlds {
    inline entt::entity createModelObject(
            entt::registry& reg, 
            glm::vec3 position, 
            glm::quat rotation, 
            AssetID meshId, 
            AssetID materialId, 
            glm::vec3 scale = glm::vec3(1.0f), 
            glm::vec4 texScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)
    ) {
        if (glm::length(rotation) == 0.0f) {
            rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        auto ent = reg.create();
        auto& transform = reg.emplace<Transform>(ent, position, rotation);
        transform.scale = scale;
        auto& worldObject = reg.emplace<WorldObject>(ent, 0, meshId);
        worldObject.texScaleOffset = texScaleOffset;
        worldObject.materials[0] = materialId;
        return ent;
    }
}
