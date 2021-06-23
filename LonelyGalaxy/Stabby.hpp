#pragma once
#include <glm/vec3.hpp>
#include <entt/entity/fwd.hpp>
#include <Core/AssetDB.hpp>

namespace lg {
    struct Stabby {
        float dragMultiplier = 0.95f;
        float dragFloor = 0.25f;
        float pulloutDistance = 0.2f;
        glm::vec3 stabDirection {0.0f, 0.0f, 1.0f};
        glm::vec3 entryPoint;
        bool embedded = false;
        entt::entity embeddedIn;
    };

    struct Stabbable {
        worlds::AssetID stabSound = ~0u;
    };
}
