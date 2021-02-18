#pragma once
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    typedef unsigned int AssetID;
    void loadScene01(PHYSFS_File* file, entt::registry& reg, bool additive);
    void loadScene02(PHYSFS_File* file, entt::registry& reg, bool additive);
    void loadScene03(PHYSFS_File* file, entt::registry& reg, bool additive);
    void loadScene04(PHYSFS_File* file, entt::registry& reg, bool additive);
}
