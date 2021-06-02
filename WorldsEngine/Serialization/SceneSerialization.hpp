#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    typedef uint32_t AssetID;

    // This class acts as a dispatcher for the two supported scene serialization formats.
    // Given a file, it will determine which format that file is and call the appropriate
    // serializer.
    class SceneLoader {
    public:
        // Loads a scene from the specified file.
        // If additive is true, the registry will be cleared before loading the scene.
        // The file handle will be closed after loading.
        static void loadScene(PHYSFS_File* file, entt::registry& reg, bool additive = false);
        static entt::entity loadEntity(PHYSFS_File* file, entt::registry& reg);
        static entt::entity createPrefab(AssetID id, entt::registry& reg);
    private:
        SceneLoader() {}
        ~SceneLoader() {}
    };

    class BinarySceneSerializer {
    public:
        static void saveScene(PHYSFS_File* file, entt::registry& reg);
        static void loadScene(PHYSFS_File* file, entt::registry& reg);

        static void saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent);
        static entt::entity loadEntity(PHYSFS_File* file, entt::registry& reg);
    private:
        BinarySceneSerializer() {}
        ~BinarySceneSerializer() {}
    };

    class JsonSceneSerializer {
    public:
        static void saveScene(PHYSFS_File* file, entt::registry& reg);
        static void loadScene(PHYSFS_File* file, entt::registry& reg);

        static void saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent);
        // Returns entt::null if the entity JSON is invalid.
        static entt::entity loadEntity(PHYSFS_File* file, entt::registry& reg);

        // Converts the specified entity to JSON.
        static std::string entityToJson(entt::registry& reg, entt::entity ent);
        // Returns entt::null if the entity JSON is invalid.
        static entt::entity jsonToEntity(entt::registry& reg, std::string json);
    private:
        JsonSceneSerializer() {}
        ~JsonSceneSerializer() {}
    };
}
