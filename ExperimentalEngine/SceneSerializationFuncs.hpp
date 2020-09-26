#pragma once
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    typedef unsigned int AssetID;
    void loadScene01(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive);
    void loadScene02(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive);
    void loadScene03(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive);
}