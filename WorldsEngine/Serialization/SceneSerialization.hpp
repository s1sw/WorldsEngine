#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    typedef uint32_t AssetID;
    void saveScene(AssetID id, entt::registry& reg);
    void saveSceneToFile(PHYSFS_File* file, entt::registry& reg);
    void deserializeScene(AssetID id, entt::registry& reg, bool additive = false);
    void deserializeScene(PHYSFS_File* file, entt::registry& reg, bool additive = false);
}
