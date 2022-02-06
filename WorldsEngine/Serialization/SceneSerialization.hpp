#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include <physfs.h>

namespace worlds {
    class DotNetScriptEngine;
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
        static entt::entity loadEntity(AssetID id, entt::registry& reg);
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

    class MessagePackSceneSerializer {
    public:
        static void saveScene(std::string path, entt::registry& reg);
        static void saveScene(AssetID path, entt::registry& reg);
        static void loadScene(PHYSFS_File* file, entt::registry& reg);
    };

    class JsonSceneSerializer {
    public:
        [[deprecated("Please use the AssetID or path variants to avoid data loss.")]]
        static void saveScene(PHYSFS_File* file, entt::registry& reg);
        static void saveScene(AssetID id, entt::registry& reg);
        static void saveScene(std::string path, entt::registry& reg);
        static void loadScene(PHYSFS_File* file, entt::registry& reg);

        static void saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent);
        // Returns entt::null if the entity JSON is invalid.
        static entt::entity loadEntity(PHYSFS_File* file, entt::registry& reg);
        static entt::entity loadEntity(AssetID id, entt::registry& reg);

        // Converts the specified entity to JSON.
        static std::string entityToJson(entt::registry& reg, entt::entity ent);
        // Returns entt::null if the entity JSON is invalid.
        static entt::entity jsonToEntity(entt::registry& reg, std::string json);

        static void setScriptEngine(DotNetScriptEngine* scriptEngine);
    private:
        JsonSceneSerializer() {}
        ~JsonSceneSerializer() {}
    };
}
