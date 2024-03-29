#include "Core/Engine.hpp"
#include <Core/WorldComponents.hpp>
#include "SceneSerialization.hpp"
#include <string>
#include <nlohmann/json.hpp>
#include <ComponentMeta/ComponentMetadata.hpp>
#include <Core/Transform.hpp>
#include <Util/TimingUtil.hpp>
#include "Core/AssetDB.hpp"
#include "robin_hood.h"
#include "slib/StaticAllocList.hpp"
#include "Scripting/NetVM.hpp"
#include <Editor/Editor.hpp>
#include <Tracy.hpp>

namespace worlds
{
    robin_hood::unordered_flat_map<AssetID, nlohmann::json> prefabCache;
    robin_hood::unordered_flat_map<entt::entity, entt::entity> idRemap;
    DotNetScriptEngine* scriptEngine;

    nlohmann::json getEntityJson(entt::entity ent, entt::registry& reg)
    {
        nlohmann::json j;

        for (auto& mdata : ComponentMetadataManager::sorted)
        {
            std::array<ENTT_ID_TYPE, 1> arr = {mdata->getComponentID()};
            auto rView = reg.runtime_view(arr.begin(), arr.end());

            if (!rView.contains(ent))
                continue;

            nlohmann::json compJ;
            mdata->toJson(ent, reg, compJ);
            if (compJ.is_null())
                continue;
            j[mdata->getName()] = compJ;
        }

        scriptEngine->serializeManagedComponents(j, ent);

        return j;
    }

    nlohmann::json getPrefabJson(AssetID id)
    {
        auto cacheIt = prefabCache.find(id);

        if (cacheIt != prefabCache.end())
        {
            return cacheIt->second;
        }
        else
        {
            // not in cache, load from disk
            PHYSFS_File* file = AssetDB::openAssetFileRead(id);
            std::string str;
            str.resize(PHYSFS_fileLength(file));
            PHYSFS_readBytes(file, str.data(), str.size());
            PHYSFS_close(file);
            nlohmann::json prefab = nlohmann::json::parse(str);
            prefabCache.insert({id, prefab});
            return prefab;
        }
    }

    nlohmann::json entityVectorToJson(const std::vector<entt::entity>& v)
    {
        nlohmann::json arr = nlohmann::json::array();

        for (const entt::entity e : v)
        {
            arr.push_back((uint32_t)e);
        }

        return arr;
    }

    void serializeEntityInScene(nlohmann::json& entities, entt::entity ent, entt::registry& reg)
    {
        nlohmann::json entity;

        if (reg.has<PrefabInstanceComponent>(ent))
        {
            PrefabInstanceComponent& pic = reg.get<PrefabInstanceComponent>(ent);
            nlohmann::json instanceJson = getEntityJson(ent, reg);
            nlohmann::json prefab = getPrefabJson(pic.prefab);

            // HACK: To avoid generating patch data for the transform, set the serialized instances's transform
            // to the prefab's.
            instanceJson["Transform"] = prefab["Transform"];
            nlohmann::json transform;
            ComponentMetadataManager::byName["Transform"]->toJson(ent, reg, transform);

            std::string path = AssetDB::idToPath(pic.prefab);

            if (path.find("SourceData/") != std::string::npos)
                path = path.substr(11);

            entity = {{"diff", nlohmann::json::diff(prefab, instanceJson)},
                        {"prefabPath", path},
                        {"Transform", transform}};
        }
        else
        {
            entity = getEntityJson(ent, reg);
        }

        entities[std::to_string((uint32_t)ent)] = entity;
    }

    nlohmann::json sceneToJsonObject(entt::registry& reg)
    {
        nlohmann::json entities;

        reg.view<Transform>(entt::exclude_t<DontSerialize>{}).each([&](entt::entity ent, Transform&) {
            serializeEntityInScene(entities, ent, reg);
        });

        nlohmann::json scene{{"entities", entities},
                             {"settings",
                              {{"skyboxPath", AssetDB::idToPath(reg.ctx<SkySettings>().skybox)},
                               {"skyboxBoost", reg.ctx<SkySettings>().skyboxBoost}}}};

        return scene;
    }

    std::string sceneToJson(entt::registry& reg)
    {
        return sceneToJsonObject(reg).dump(2);
    }

    void JsonSceneSerializer::saveScene(PHYSFS_File* file, entt::registry& reg)
    {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void JsonSceneSerializer::saveScene(AssetID id, entt::registry& reg)
    {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_File* file = AssetDB::openAssetFileWrite(id);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void JsonSceneSerializer::saveScene(std::string path, entt::registry& reg)
    {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_File* file = PHYSFS_openWrite(path.c_str());
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void MessagePackSceneSerializer::saveScene(std::string path, entt::registry& reg)
    {
        nlohmann::json j = sceneToJsonObject(reg);
        std::vector<uint8_t> data = nlohmann::json::to_msgpack(j);

        PHYSFS_File* file = PHYSFS_openWrite(path.c_str());

        const char header[5] = "WMSP";

        PHYSFS_writeBytes(file, header, 4);
        PHYSFS_writeBytes(file, data.data(), data.size());
        PHYSFS_close(file);
    }

    void MessagePackSceneSerializer::saveScene(AssetID id, entt::registry& reg)
    {
        saveScene(AssetDB::idToPath(id), reg);
    }

    struct ComponentDeserializationInfo
    {
        std::string id;
        bool isNative;
    };

    void deserializeEntityComponents(const nlohmann::json& j, entt::registry& reg, entt::entity ent)
    {
        ZoneScoped;
        slib::StaticAllocList<ComponentDeserializationInfo> componentIds(j.size());

        for (auto& compPair : j.items())
        {
            ComponentDeserializationInfo cdsi;
            cdsi.id = compPair.key();
            cdsi.isNative =
                ComponentMetadataManager::byName.find(compPair.key()) != ComponentMetadataManager::byName.end();
            componentIds.add(std::move(cdsi));
        }

        {
            ZoneScopedN("Component sort");
            std::sort(componentIds.begin(), componentIds.end(), [](const auto& a, const auto& b) {
                if (a.isNative && b.isNative)
                    return ComponentMetadataManager::byName.at(a.id)->getSortID() <
                        ComponentMetadataManager::byName.at(b.id)->getSortID();
                else if (a.isNative && !b.isNative)
                    return true;
                else if (!a.isNative && b.isNative)
                    return false;
                else
                    return false;
            });
        }

        for (auto& cdsi : componentIds)
        {
            if (cdsi.isNative)
            {
                ZoneScopedN("deserializeEntityComponents fromJson");
                auto compMeta = ComponentMetadataManager::byName.at(cdsi.id);
                compMeta->fromJson(ent, reg, idRemap, j[cdsi.id]);
            }
            else
            {
                auto& componentJson = j[cdsi.id];
                scriptEngine->deserializeManagedComponent(cdsi.id.c_str(), componentJson, ent);
            }
        }
    }

    entt::entity createJsonEntity(const nlohmann::json& j, entt::registry& reg, entt::entity id)
    {
        entt::entity ent = reg.create(id);
        deserializeEntityComponents(j, reg, ent);

        if (!reg.has<Transform>(ent))
        {
            logErr("Not deserializing entity because it lacks a transform");
            reg.destroy(ent);
            return entt::null;
        }

        return ent;
    }

    entt::entity createJsonEntity(const nlohmann::json& j, entt::registry& reg)
    {
        entt::entity ent = reg.create();
        deserializeEntityComponents(j, reg, ent);

        if (!reg.has<Transform>(ent))
        {
            logErr("Not deserializing entity because it lacks a transform");
            reg.destroy(ent);
            return entt::null;
        }

        return ent;
    }

    // Loads entities into the specified registry.
    // j is the array of entities to load.
    void loadSceneEntities(entt::registry& reg, const nlohmann::json& j)
    {
        ZoneScoped;
        logMsg("scene has %lu entities", j.size());
        idRemap.clear();

        // 1. Create all the scene's entities and deserialize transforms
        for (const auto& p : j.items())
        {
            entt::entity id = (entt::entity)std::stoul(p.key());
            entt::entity newEnt = reg.create(id);

            idRemap.insert({id, newEnt});
            // Even though the ID map isn't complete, it's fine to pass it in here since transforms don't need it
            ComponentMetadataManager::byName["Transform"]->fromJson(newEnt, reg, idRemap, p.value()["Transform"]);
        }

        // 2. Create patched KVs for entities
        std::vector<std::pair<entt::entity, nlohmann::json>> entityComponentData;
        for (const auto& p : j.items())
        {
            entt::entity newEnt = idRemap[(entt::entity)std::stoul(p.key())];
            if (p.value().contains("prefabPath"))
            {
                std::string prefabPath = p.value()["prefabPath"].get<std::string>();
                AssetID prefabId = AssetDB::pathToId(prefabPath);

                nlohmann::json components = getPrefabJson(prefabId);
                try
                {
                    components = components.patch(p.value()["diff"]);
                }
                catch (nlohmann::detail::out_of_range& ex)
                {
                    logErr("Malformed prefab instance! Resetting... (%s)", ex.what());
                }

                if (p.value().contains("Transform"))
                {
                    components["Transform"] = p.value()["Transform"];
                }
                entityComponentData.push_back({ newEnt, components });
            }
            else
            {
                entityComponentData.push_back({ newEnt, p.value() });
            }
        }

        // 3. Deserialize in component order
        for (ComponentMetadata* cm : ComponentMetadataManager::sorted)
        {
            for (auto& pair : entityComponentData)
            {
                if (pair.second.contains(cm->getName()))
                {
                    cm->fromJson(pair.first, reg, idRemap, pair.second[cm->getName()]);
                }
            }
        }

        // 4. Deserialize managed components
        for (auto& pair : entityComponentData)
        {
            for (auto& componentPair : pair.second.items())
            {
                if (ComponentMetadataManager::byName.count(componentPair.key()) == 0)
                {
                    auto& componentJson = componentPair.value();
                    scriptEngine->deserializeManagedComponent(componentPair.key().c_str(), componentJson, pair.first);
                }
            }
        }
    }

    void deserializeJsonScene(nlohmann::json& j, entt::registry& reg)
    {
        if (!j.contains("entities"))
        {
            loadSceneEntities(reg, j);
        }
        else
        {
            loadSceneEntities(reg, j["entities"]);
            SkySettings settings{};
            settings.skybox = AssetDB::pathToId(j["settings"]["skyboxPath"].get<std::string>());
            settings.skyboxBoost = j["settings"].value("skyboxBoost", 1.0f);
            reg.set<SkySettings>(settings);
        }
    }

    void JsonSceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg)
    {
        PerfTimer timer;
        try
        {
            prefabCache.clear();
            idRemap.clear();
            std::string str;
            str.resize(PHYSFS_fileLength(file));
            PHYSFS_readBytes(file, str.data(), str.size());

            nlohmann::json j = nlohmann::json::parse(str);
            j.type();
            deserializeJsonScene(j, reg);

            logMsg("loaded json scene in %.3fms", timer.stopGetMs());
        }
        catch (nlohmann::detail::exception& ex)
        {
            logErr("Failed to load scene: %s", ex.what());
        }
    }

    void MessagePackSceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg)
    {
        PerfTimer timer;

        char maybeHeader[5] = "____";

        PHYSFS_readBytes(file, maybeHeader, 4);

        if (strcmp(maybeHeader, "WMSP") != 0)
        {
            logErr("Scene file had incorrect header for msgpack");
            return;
        }

        try
        {
            prefabCache.clear();

            std::vector<uint8_t> dat;
            // subtract header length
            dat.resize(PHYSFS_fileLength(file) - 4);
            PHYSFS_readBytes(file, dat.data(), dat.size());

            nlohmann::json j = nlohmann::json::from_msgpack(dat.begin(), dat.end());
            deserializeJsonScene(j, reg);

            logMsg("loaded msgpack scene in %.3fms", timer.stopGetMs());
        }
        catch (nlohmann::detail::exception& ex)
        {
            logErr("Failed to load scene: %s", ex.what());
        }
    }

    void JsonSceneSerializer::saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent)
    {
        std::string jsonStr = entityToJson(reg, ent);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
    }

    entt::entity JsonSceneSerializer::loadEntity(PHYSFS_File* file, entt::registry& reg)
    {
        std::string str;
        str.resize(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, str.data(), str.size());
        return jsonToEntity(reg, str);
    }

    entt::entity JsonSceneSerializer::loadEntity(AssetID id, entt::registry& reg)
    {
        auto cacheIt = prefabCache.find(id);

        if (cacheIt != prefabCache.end())
        {
            return createJsonEntity(cacheIt->second, reg);
        }

        // not in cache, load from disk
        PHYSFS_File* file = AssetDB::openAssetFileRead(id);
        std::string str;
        str.resize(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, str.data(), str.size());
        PHYSFS_close(file);

        nlohmann::json j = nlohmann::json::parse(str);
        prefabCache.insert({id, j});

        return createJsonEntity(j, reg);
    }

    std::string JsonSceneSerializer::entityToJson(entt::registry& reg, entt::entity ent)
    {
        nlohmann::json j = getEntityJson(ent, reg);
        return j.dump();
    }

    entt::entity JsonSceneSerializer::jsonToEntity(entt::registry& reg, std::string jText)
    {
        auto j = nlohmann::json::parse(jText);
        return createJsonEntity(j, reg);
    }

    std::string JsonSceneSerializer::entitiesToJson(entt::registry& reg, entt::entity* entities, size_t entityCount)
    {
        nlohmann::json entitiesJ;

        for (size_t i = 0; i < entityCount; i++)
        {
            serializeEntityInScene(entitiesJ, entities[i], reg);
        }

        return entitiesJ.dump();
    }

    std::vector<entt::entity> JsonSceneSerializer::jsonToEntities(entt::registry& reg, std::string json)
    {
        nlohmann::json j = nlohmann::json::parse(json);

        if (!j.is_object())
        {
            logErr("Invalid multiple entity json");
            return std::vector<entt::entity>{};
        }

        loadSceneEntities(reg, j);

        std::vector<entt::entity> loadedEntities;

        for (const auto& p : j.items())
        {
            loadedEntities.push_back(idRemap[(entt::entity)std::stoul(p.key())]);
        }

        return loadedEntities;
    }

    void JsonSceneSerializer::setScriptEngine(DotNetScriptEngine* scriptEngine)
    {
        worlds::scriptEngine = scriptEngine;
    }
}
