#pragma once
#include <cstdint>
#include <entt/entt.hpp>

namespace worlds {
    typedef uint32_t AssetID;
    void saveScene(AssetID id, entt::registry& reg);
    void deserializeScene(AssetID id, entt::registry& reg, bool additive = false);
}