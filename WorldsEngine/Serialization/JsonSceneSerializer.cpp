#include "Core/Engine.hpp"
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
#include <tracy/Tracy.hpp>

namespace worlds {
    robin_hood::unordered_flat_map<AssetID, nlohmann::json> prefabCache;
    DotNetScriptEngine* scriptEngine;

    nlohmann::json getEntityJson(entt::entity ent, entt::registry& reg)  {
        nlohmann::json j;

        for (auto& mdata : ComponentMetadataManager::sorted) {
            std::array<ENTT_ID_TYPE, 1> arr = { mdata->getComponentID() };
            auto rView = reg.runtime_view(arr.begin(), arr.end());

            if (!rView.contains(ent)) continue;

            nlohmann::json compJ;
            mdata->toJson(ent, reg, compJ);
            if (compJ.is_null()) continue;
            j[mdata->getName()] = compJ;
        }

        scriptEngine->serializeManagedComponents(j, ent);

        return j;
    }

    nlohmann::json getPrefabJson(AssetID id) {
        auto cacheIt = prefabCache.find(id);

        if (cacheIt != prefabCache.end()) {
            return cacheIt->second;
        } else {
            // not in cache, load from disk
            PHYSFS_File* file = AssetDB::openAssetFileRead(id);
            std::string str;
            str.resize(PHYSFS_fileLength(file));
            PHYSFS_readBytes(file, str.data(), str.size());
            PHYSFS_close(file);
            nlohmann::json prefab = nlohmann::json::parse(str);
            prefabCache.insert({ id, prefab });
            return prefab;
        }
    }

    nlohmann::json entityVectorToJson(const std::vector<entt::entity>& v) {
        nlohmann::json arr = nlohmann::json::array();

        for (const entt::entity e : v) {
            arr.push_back((uint32_t)e);
        }

        return arr;
    }

    nlohmann::json getJsonForFolder(const EntityFolder& folder) {
        nlohmann::json children = nlohmann::json::array();
        for (const EntityFolder& f : folder.children) {
            children.push_back(getJsonForFolder(f));
        }

        return nlohmann::json {
            { "name", folder.name },
            { "entities", entityVectorToJson(folder.entities) },
            { "children", children }
        };
    }

    EntityFolder folderFromJson(nlohmann::json j) {
        EntityFolder f{ j["name"] };

        for (uint32_t v : j["entities"]) {
            f.entities.push_back((entt::entity)v);
        }

        for (nlohmann::json& c : j["children"]) {
            f.children.push_back(folderFromJson(c));
        }

        return f;
    }

    nlohmann::json sceneToJsonObject(entt::registry& reg) {
        nlohmann::json entities;

        reg.view<Transform>(entt::exclude_t<DontSerialize>{}).each([&](entt::entity ent, Transform&) {
            nlohmann::json entity;

            if (reg.has<PrefabInstanceComponent>(ent)) {
                PrefabInstanceComponent& pic = reg.get<PrefabInstanceComponent>(ent);
                nlohmann::json instanceJson = getEntityJson(ent, reg);
                nlohmann::json prefab = getPrefabJson(pic.prefab);

                // HACK: To avoid generating patch data for the transform, set the serialized instances's transform
                // to the prefab's.
                instanceJson["Transform"] = prefab["Transform"];
                nlohmann::json transform;
                ComponentMetadataManager::byName["Transform"]->toJson(ent, reg, transform);

                entity = {
                    { "diff", nlohmann::json::diff(prefab, instanceJson) },
                    { "prefabPath", AssetDB::idToPath(pic.prefab) },
                    { "Transform", transform }
                };
            } else {
                entity = getEntityJson(ent, reg);
            }

            entities[std::to_string((uint32_t)ent)] = entity;
        });

        nlohmann::json scene {
            { "entities", entities },
            { "settings", {
                { "skyboxPath", AssetDB::idToPath(reg.ctx<SceneSettings>().skybox) },
                { "skyboxBoost", reg.ctx<SceneSettings>().skyboxBoost }
            }}
        };

        EntityFolders* entityFolders = reg.try_ctx<EntityFolders>();

        if (entityFolders) {
            scene["rootEntityFolder"] = getJsonForFolder(entityFolders->rootFolder);
        }

        return scene;
    }

    std::string sceneToJson(entt::registry& reg) {
        return sceneToJsonObject(reg).dump(2);
    }

    void JsonSceneSerializer::saveScene(PHYSFS_File* file, entt::registry& reg) {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void JsonSceneSerializer::saveScene(AssetID id, entt::registry& reg) {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_File* file = AssetDB::openAssetFileWrite(id);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void JsonSceneSerializer::saveScene(std::string path, entt::registry& reg) {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_File* file = PHYSFS_openWrite(path.c_str());
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void MessagePackSceneSerializer::saveScene(std::string path, entt::registry& reg) {
        nlohmann::json j = sceneToJsonObject(reg);
        std::vector<uint8_t> data = nlohmann::json::to_msgpack(j);

        PHYSFS_File* file = PHYSFS_openWrite(path.c_str());

        const char header[5] = "WMSP";

        PHYSFS_writeBytes(file, header, 4);
        PHYSFS_writeBytes(file, data.data(), data.size());
        PHYSFS_close(file);
    }

    struct ComponentDeserializationInfo {
        std::string id;
        bool isNative;
    };

    void deserializeEntityComponents(const nlohmann::json& j, entt::registry& reg, entt::entity ent) {
        ZoneScoped;
        slib::StaticAllocList<ComponentDeserializationInfo> componentIds(j.size());

        for (auto& compPair : j.items()) {
            ComponentDeserializationInfo cdsi;
            cdsi.id = compPair.key();
            cdsi.isNative = ComponentMetadataManager::byName.find(compPair.key()) != ComponentMetadataManager::byName.end();
            componentIds.add(std::move(cdsi));
        }

        std::sort(componentIds.begin(), componentIds.end(), [](const auto& a, const auto& b) {
            if (a.isNative && b.isNative)
                return ComponentMetadataManager::byName.at(a.id)->getSortID() < ComponentMetadataManager::byName.at(b.id)->getSortID();
            else if (a.isNative && !b.isNative)
                return true;
            else if (!a.isNative && b.isNative)
                return false;
            else
                return false;
        });

        for (auto& cdsi : componentIds) {
            if (cdsi.isNative) {
                ZoneScopedN("deserializeEntityComponents fromJson");
                auto compMeta = ComponentMetadataManager::byName.at(cdsi.id);
                compMeta->fromJson(ent, reg, j[cdsi.id]);
            } else {
                auto componentJson = j[cdsi.id];
                scriptEngine->deserializeManagedComponent(cdsi.id.c_str(), componentJson, ent);
            }
        }
    }

    entt::entity createJsonEntity(const nlohmann::json& j, entt::registry& reg, entt::entity id) {
        entt::entity ent = reg.create(id);
        deserializeEntityComponents(j, reg, ent);

        if (!reg.has<Transform>(ent)) {
            logErr("Not deserializing entity because it lacks a transform");
            reg.destroy(ent);
            return entt::null;
        }

        return ent;
    }

    entt::entity createJsonEntity(const nlohmann::json& j, entt::registry& reg) {
        entt::entity ent = reg.create();
        deserializeEntityComponents(j, reg, ent);

        if (!reg.has<Transform>(ent)) {
            logErr("Not deserializing entity because it lacks a transform");
            reg.destroy(ent);
            return entt::null;
        }

        return ent;
    }

    // Loads entities into the specified registry.
    // j is the array of entities to load.
    void loadSceneEntities(entt::registry& reg, const nlohmann::json& j) {
        ZoneScoped;
        logMsg("scene has %lu entities", j.size());
        // 1. Create all the scene's entities
        for (const auto& p : j.items()) {
            entt::entity id = (entt::entity)std::stoul(p.key());
            entt::entity newEnt = reg.create(id);

            if (id != newEnt) {
                logErr("failed to deserialize");
                return;
            }
        }

        // 2. Load prefabs
        for (const auto& p : j.items()) {
            entt::entity newEnt = (entt::entity)std::stoul(p.key());

            if (p.value().contains("prefabPath")) {
                std::string prefabPath = p.value()["prefabPath"].get<std::string>();
                AssetID prefabId = AssetDB::pathToId(prefabPath);

                nlohmann::json components = getPrefabJson(prefabId);
                try {
                    components = components.patch(p.value()["diff"]);
                } catch (nlohmann::detail::out_of_range& ex) {
                    if (ex.id == 403) {
                        logErr("Malformed prefab instance!");
                    } else {
                        throw ex;
                    }
                }

                if (p.value().contains("Transform")) {
                    components["Transform"] = p.value()["Transform"];
                }

                deserializeEntityComponents(components, reg, newEnt);

                reg.emplace<PrefabInstanceComponent>(newEnt).prefab = prefabId;
            }
        }

        struct PrioritisedEntity {
            entt::entity ent;
            int maxComponentSort;
        };

        slib::StaticAllocList<PrioritisedEntity> prioritisedEntities(j.size());

        // 3. Determine max sort ID of each component
        for (const auto& p : j.items()) {
            entt::entity newEnt = (entt::entity)std::stoul(p.key());
            if (p.value().contains("prefabPath")) continue;

            int maxSort = 0;
            for (const auto& c : p.value().items()) {
                if (ComponentMetadataManager::byName.count(c.key()) == 0) continue;
                ComponentEditor* meta = ComponentMetadataManager::byName[c.key()];

                maxSort = std::max(maxSort, meta->getSortID());
            }

            prioritisedEntities.add({ newEnt, maxSort });
        }

        {
            ZoneScopedN("Sort prioritised entities");
            // 4. Sort by max sort ID
            // This way entities with a component with a high sort ID will be deserialized
            // after those with a low sort ID
            std::sort(prioritisedEntities.begin(), prioritisedEntities.end(), [](PrioritisedEntity& a, PrioritisedEntity& b) {
                return a.maxComponentSort < b.maxComponentSort;
                });
        }

        {
            ZoneScopedN("component deserialization");
            for (worlds::ComponentEditor* meta : ComponentMetadataManager::sorted) {
                for (PrioritisedEntity& pe : prioritisedEntities) {
                    entt::entity newEnt = pe.ent;
                    const auto& entityJson = j[std::to_string((uint32_t)newEnt)];

                    if (entityJson.contains(meta->getName())) {
                        ZoneScopedN("meta->fromJson");
                        meta->fromJson(newEnt, reg, entityJson[meta->getName()]);
                    }
                }
            }
        }

        // 5. Deserialize each managed component
        // This is super inefficient, but it preserves initialisation order
        for (PrioritisedEntity& pe : prioritisedEntities) {
            entt::entity newEnt = pe.ent;
            const auto& entityJson = j[std::to_string((uint32_t)newEnt)];

            for (const auto& v : entityJson.items()) {
                if (ComponentMetadataManager::byName.count(v.key()) == 0) {
                    scriptEngine->deserializeManagedComponent(v.key().c_str(), v.value(), newEnt);
                }
            }
        }
    }

    void validateFolder(EntityFolder& folder, entt::registry& registry) {
        folder.entities.erase(
            std::remove_if(
                folder.entities.begin(),
                folder.entities.end(),
                [&registry](entt::entity e) { return !registry.valid(e); }
            ), folder.entities.end());

        for (EntityFolder& e : folder.children) {
            validateFolder(e, registry);
        }
    }

    void deserializeJsonScene(nlohmann::json& j, entt::registry& reg) {
        if (!j.contains("entities")) {
            loadSceneEntities(reg, j);
        } else {
            loadSceneEntities(reg, j["entities"]);
            SceneSettings settings{};
            settings.skybox = AssetDB::pathToId(j["settings"]["skyboxPath"].get<std::string>());
            settings.skyboxBoost = j["settings"].value("skyboxBoost", 1.0f);
            reg.set<SceneSettings>(settings);

            if (j.contains("rootEntityFolder")) {
                EntityFolders folders;
                folders.rootFolder = folderFromJson(j["rootEntityFolder"]);
                validateFolder(folders.rootFolder, reg);
                reg.set<EntityFolders>(folders);
            }
        }
    }

    void JsonSceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg) {
        PerfTimer timer;
        try {
            prefabCache.clear();
            std::string str;
            str.resize(PHYSFS_fileLength(file));
            PHYSFS_readBytes(file, str.data(), str.size());

            nlohmann::json j = nlohmann::json::parse(str);
            deserializeJsonScene(j, reg);

            logMsg("loaded json scene in %.3fms", timer.stopGetMs());
        } catch (nlohmann::detail::exception& ex) {
            logErr("Failed to load scene: %s", ex.what());
        }
    }

    void MessagePackSceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg) {
        PerfTimer timer;

        char maybeHeader[5] = "____";

        PHYSFS_readBytes(file, maybeHeader, 4);

        if (strcmp(maybeHeader, "WMSP") != 0) {
            logErr("Scene file had incorrect header for msgpack");
            return;
        }

        try {
            prefabCache.clear();

            std::vector<uint8_t> dat;
            // subtract header length
            dat.resize(PHYSFS_fileLength(file) - 4);
            PHYSFS_readBytes(file, dat.data(), dat.size());

            nlohmann::json j = nlohmann::json::from_msgpack(dat.begin(), dat.end());
            deserializeJsonScene(j, reg);

            logMsg("loaded msgpack scene in %.3fms", timer.stopGetMs());
        } catch (nlohmann::detail::exception& ex) {
            logErr("Failed to load scene: %s", ex.what());
        }
    }

    void JsonSceneSerializer::saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent) {
        std::string jsonStr = entityToJson(reg, ent);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
    }

    entt::entity JsonSceneSerializer::loadEntity(PHYSFS_File* file, entt::registry& reg) {
        std::string str;
        str.resize(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, str.data(), str.size());
        return jsonToEntity(reg, str);
    }


    entt::entity JsonSceneSerializer::loadEntity(AssetID id, entt::registry& reg) {
        auto cacheIt = prefabCache.find(id);

        if (cacheIt != prefabCache.end()) {
            return createJsonEntity(cacheIt->second, reg);
        }

        // not in cache, load from disk
        PHYSFS_File* file = AssetDB::openAssetFileRead(id);
        std::string str;
        str.resize(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, str.data(), str.size());
        PHYSFS_close(file);

        nlohmann::json j = nlohmann::json::parse(str);
        prefabCache.insert({ id, j });

        return createJsonEntity(j, reg);
    }

    std::string JsonSceneSerializer::entityToJson(entt::registry& reg, entt::entity ent) {
        nlohmann::json j = getEntityJson(ent, reg);
        return j.dump();
    }

    entt::entity JsonSceneSerializer::jsonToEntity(entt::registry& reg, std::string jText) {
        auto j = nlohmann::json::parse(jText);
        return createJsonEntity(j, reg);
    }

    void JsonSceneSerializer::setScriptEngine(DotNetScriptEngine* scriptEngine) {
        worlds::scriptEngine = scriptEngine;
    }
}
