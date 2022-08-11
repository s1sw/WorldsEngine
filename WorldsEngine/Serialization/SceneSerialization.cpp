#include "SceneSerialization.hpp"
#include "../Core/AssetDB.hpp"
#include "../Core/Log.hpp"
#include "Core/Engine.hpp"
#include "Core/WorldComponents.hpp"
#include <Audio/Audio.hpp>
#include <Navigation/Navigation.hpp>
#include <physfs.h>
#include <Tracy.hpp>

namespace worlds
{
    void clearEntities(entt::registry& reg)
    {
        std::vector<entt::entity> entitiesToClear;
        reg.view<Transform>(entt::exclude_t<KeepOnSceneLoad>{}).each([&](entt::entity e, Transform&) {
            entitiesToClear.push_back(e);
        });

        for (entt::entity e : entitiesToClear)
        {
            reg.destroy(e, 0);
        }
    }

    // Do basic checks on the first byte to determine
    // the most appropriate scene serializer to call.
    void SceneLoader::loadScene(PHYSFS_File* file, entt::registry& reg, bool additive)
    {
        ZoneScoped;
        if (PHYSFS_fileLength(file) <= 4)
        {
            logErr(WELogCategoryEngine, "Scene file was too short.");
            return;
        }

        unsigned char firstByte;
        PHYSFS_readBytes(file, &firstByte, 1);
        PHYSFS_seek(file, 0);

        // check first character of magic to determine WSCN or ESCN
        if (firstByte == 'W' || firstByte == 'E')
        {
            if (!additive)
            {
                clearEntities(reg);
            }

            // Next up, check if it's the old binary format or WMSP
            char maybeHeader[5] = "____";

            PHYSFS_readBytes(file, maybeHeader, 4);
            PHYSFS_seek(file, 0);

            if (strcmp(maybeHeader, "WMSP") == 0)
            {
                MessagePackSceneSerializer::loadScene(file, reg);
            }
            else
            {
                logErr("Unrecognised scene header: %s", maybeHeader);
            }
        }
        else if (firstByte == '{')
        {
            if (!additive)
            {
                clearEntities(reg);
            }

            JsonSceneSerializer::loadScene(file, reg);
        }
        else
        {
            logErr(WELogCategoryEngine, "Scene has unrecognized file format");
            PHYSFS_close(file);
            return;
        }

        PHYSFS_close(file);
        AudioSystem::getInstance()->updateAudioScene(reg);
        NavigationSystem::updateNavMesh(reg);
    }

    entt::entity SceneLoader::loadEntity(PHYSFS_File* file, entt::registry& reg)
    {
        return JsonSceneSerializer::loadEntity(file, reg);
    }

    entt::entity SceneLoader::createPrefab(AssetID id, entt::registry& reg)
    {
        if (id == INVALID_ASSET)
        {
            logWarn(WELogCategoryScripting, "Invalid asset ID!");
            return entt::null;
        }

        entt::entity ent = JsonSceneSerializer::loadEntity(id, reg);
        if (reg.valid(ent))
        {
            PrefabInstanceComponent& pic = reg.emplace<PrefabInstanceComponent>(ent);
            pic.prefab = id;
        }
        return ent;
    }
}
