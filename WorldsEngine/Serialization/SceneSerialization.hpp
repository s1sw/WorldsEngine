#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    typedef uint32_t AssetID;
    void saveScene(AssetID id, entt::registry& reg);
    void saveSceneToFile(PHYSFS_File* file, entt::registry& reg);
    void saveSceneJson(AssetID id, entt::registry& reg);
    void saveSceneToFileJson(PHYSFS_File* file, entt::registry& reg);
    void deserializeScene(AssetID id, entt::registry& reg, bool additive = false);
    void deserializeScene(PHYSFS_File* file, entt::registry& reg, bool additive = false);
    std::string entityToJson(entt::registry& reg, entt::entity ent);
    std::string sceneToJson(entt::registry& reg);
}
