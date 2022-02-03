#include "SceneSerialization.hpp"
#include "../Core/Log.hpp"
#include "../Core/AssetDB.hpp"
#include <physfs.h>
#include "Core/Engine.hpp"
#include <Audio/Audio.hpp>
#include <tracy/Tracy.hpp>

namespace worlds {
    // Do basic checks on the first byte to determine
    // the most appropriate scene serializer to call.
    void SceneLoader::loadScene(PHYSFS_File* file, entt::registry& reg, bool additive) {
        ZoneScoped;
        if (PHYSFS_fileLength(file) <= 4) {
            logErr(WELogCategoryEngine, "Scene file was too short.");
            return;
        }

        unsigned char firstByte;
        PHYSFS_readBytes(file, &firstByte, 1);
        PHYSFS_seek(file, 0);

        // check first character of magic to determine WSCN or ESCN
        if (firstByte == 'W' || firstByte == 'E') {
            if (!additive)
                reg.clear();

            // Next up, check if it's the old binary format or WMSP
            char maybeHeader[5] = "____";

            PHYSFS_readBytes(file, maybeHeader, 4);
            PHYSFS_seek(file, 0);

            if (strcmp(maybeHeader, "WMSP") == 0) {
                MessagePackSceneSerializer::loadScene(file, reg);
            } else {
                BinarySceneSerializer::loadScene(file, reg);
            }
        } else if (firstByte == '{') {
            if (!additive)
                reg.clear();

            JsonSceneSerializer::loadScene(file, reg);
        } else {
            logErr(WELogCategoryEngine, "Scene has unrecognized file format");
            PHYSFS_close(file);
            return;
        }


        PHYSFS_close(file);
        AudioSystem::getInstance()->updateAudioScene(reg);
    }

    entt::entity SceneLoader::loadEntity(PHYSFS_File* file, entt::registry& reg) {
        return JsonSceneSerializer::loadEntity(file, reg);
    }

    entt::entity SceneLoader::createPrefab(AssetID id, entt::registry& reg) {
        entt::entity ent = JsonSceneSerializer::loadEntity(id, reg);
        if (reg.valid(ent)) {
            PrefabInstanceComponent& pic = reg.emplace<PrefabInstanceComponent>(ent);
            pic.prefab = id;
        }
        return ent;
    }
}
