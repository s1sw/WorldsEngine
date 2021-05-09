#include "SceneSerialization.hpp"
#include <string>
#include <nlohmann/json.hpp>
#include "../ComponentMeta/ComponentMetadata.hpp"
#include "../Core/Transform.hpp"
#include "../Util/TimingUtil.hpp"

namespace worlds {
    std::string sceneToJson(entt::registry& reg) {
        nlohmann::json j;

        reg.view<Transform>().each([&](entt::entity ent, Transform&) {
            nlohmann::json entity;

            for (auto& mdata : ComponentMetadataManager::sorted) {
                std::array<ENTT_ID_TYPE, 1> arr = { mdata->getComponentID() };
                auto rView = reg.runtime_view(arr.begin(), arr.end());

                if (!rView.contains(ent)) continue;

                nlohmann::json compJ;
                mdata->toJson(ent, reg, compJ);
                entity[mdata->getName()] = compJ;
            }

            j[std::to_string((uint32_t)ent)] = entity;
        });

        return j.dump(2);
    }

    void JsonSceneSerializer::saveScene(PHYSFS_File* file, entt::registry& reg) {
        std::string jsonStr = sceneToJson(reg);
        PHYSFS_writeBytes(file, jsonStr.data(), jsonStr.size());
        PHYSFS_close(file);
    }

    void JsonSceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg) {
        PerfTimer timer;
        std::string str;
        str.resize(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, str.data(), str.size());

        nlohmann::json j = nlohmann::json::parse(str);

        logMsg("scene has %lu entities", j.size());
        for (auto& p : j.items()) {
            auto newEnt = reg.create((entt::entity)std::stoul(p.key()));

            std::vector<std::string> componentIds;

            for (auto& compPair : p.value().items()) {
                componentIds.push_back(compPair.key());
            }

            std::sort(componentIds.begin(), componentIds.end(), [](std::string a, std::string b) {
                return ComponentMetadataManager::byName.at(a)->getSortID() < ComponentMetadataManager::byName.at(b)->getSortID();
            });

            for (auto& id : componentIds) {
                auto compMeta = ComponentMetadataManager::byName.at(id);
                compMeta->fromJson(newEnt, reg, p.value()[id]);
            }
        }

        logMsg("loaded json scene in %.3fms", timer.stopGetMs());
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

    std::string JsonSceneSerializer::entityToJson(entt::registry& reg, entt::entity ent) {
        nlohmann::json j;
        for (auto& mdata : ComponentMetadataManager::sorted) {
            std::array<ENTT_ID_TYPE, 1> arr = { mdata->getComponentID() };
            auto rView = reg.runtime_view(arr.begin(), arr.end());

            if (!rView.contains(ent)) continue;

            nlohmann::json compJ;
            mdata->toJson(ent, reg, compJ);
            j[mdata->getName()] = compJ;
        }

        return j.dump();
    }

    entt::entity JsonSceneSerializer::jsonToEntity(entt::registry& reg, std::string jText) {
        auto j = nlohmann::json::parse(jText);
        entt::entity ent = reg.create();

        std::vector<std::string> componentIds;

        for (auto& compPair : j.items()) {
            if (ComponentMetadataManager::byName.find(compPair.key()) != ComponentMetadataManager::byName.end())
                componentIds.push_back(compPair.key());
            else
                logMsg("Unknown component ID \"%s\"", compPair.key());
        }

        std::sort(componentIds.begin(), componentIds.end(), [](std::string a, std::string b) {
            return ComponentMetadataManager::byName.at(a)->getSortID() < ComponentMetadataManager::byName.at(b)->getSortID();
        });

        for (auto& id : componentIds) {
            auto compMeta = ComponentMetadataManager::byName.at(id);
            compMeta->fromJson(ent, reg, j[id]);
        }

        if (!reg.has<Transform>(ent)) {
            logErr("Not deserializing clipboard entity because it lacks a transform");
            reg.destroy(ent);
            return entt::null;
        }

        return ent;
    }
}
