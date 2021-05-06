#include "ResourceSlots.hpp"
#include "../Core/Engine.hpp"
#include "tracy/Tracy.hpp"
#include <physfs.h>
#include <optional>
#include "../Util/JsonUtil.hpp"
#include <nlohmann/json.hpp>

namespace glm {
    void to_json(nlohmann::json& j, const glm::vec3& vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z};
    }

    void from_json(const nlohmann::json& j, glm::vec3& vec) {
        vec = glm::vec3{j[0], j[1], j[2]};
    }
}

namespace worlds {
    std::optional<std::string> getString(const sajson::value& obj, const char* key) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return std::nullopt;

        return obj.get_object_value(idx).as_string();
    }

    uint32_t MaterialSlots::getTexture(nlohmann::json& j, std::string key) {
        auto it = j.find(key);

        if (it == j.end()) return ~0u;

        return texSlots.loadOrGet(g_assetDB.addOrGetExisting(it.value()));
    }

    void MaterialSlots::parseMaterial(AssetID asset, PackedMaterial& mat, MatExtraData& extraDat) {
        ZoneScoped;
        PHYSFS_File* f = g_assetDB.openAssetFileRead(asset);

        if (f == nullptr) {
            std::string path = g_assetDB.getAssetPath(asset);
            auto err = PHYSFS_getLastErrorCode();
            auto errStr = PHYSFS_getErrorByCode(err);
            logErr(WELogCategoryRender, "Failed to open %s: %s", path.c_str(), errStr);
            f = PHYSFS_openRead("Materials/missing.json");
        }

        size_t fileSize = PHYSFS_fileLength(f);
        std::string str;
        str.resize(fileSize);
        // Add a null byte to the end to make a C string
        PHYSFS_readBytes(f, str.data(), fileSize);
        PHYSFS_close(f);

        try {
            auto j = nlohmann::json::parse(str);

            if (j.type() != nlohmann::detail::value_t::object) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid material document");
                return;
            }

            auto albedoPath = j["albedoPath"];

            mat.setCutoff(j.value("alphaCutoff", 0.0));
            mat.setFlags(MaterialFlags::None);

            mat.metallic = j["metallic"];
            mat.roughness = j["roughness"];
            mat.albedoColor = j["albedoColor"].get<glm::vec3>();

            if (j.find("emissiveColor") != j.end()) {
                mat.emissiveColor = j["emissiveColor"].get<glm::vec3>();

                // Ignore tiny emmisive values
                if (glm::compAdd(mat.emissiveColor) < 0.003f) {
                    mat.emissiveColor = glm::vec3 {0.0f};
                }
            } else {
                mat.emissiveColor = glm::vec3 {0.0f};
            }

            auto albedoAssetId = g_assetDB.addOrGetExisting(albedoPath);

            uint32_t nMapSlot = getTexture(j, "normalMapPath");
            uint32_t hMapSlot = getTexture(j, "heightmapPath");

            mat.albedoTexIdx = texSlots.loadOrGet(albedoAssetId);
            mat.normalTexIdx = nMapSlot;
            mat.heightmapTexIdx = hMapSlot;

            auto metalMap = getTexture(j, "metalMapPath");
            auto roughMap = getTexture(j, "roughMapPath");
            auto aoMap = getTexture(j, "aoMapPath");
            auto pbrMap = getTexture(j, "pbrMapPath");

            if (pbrMap != ~0u) {
                mat.roughTexIdx = pbrMap;
                mat.setFlags(mat.getFlags() | MaterialFlags::UsePackedPBR);
            } else {
                mat.metalTexIdx = metalMap;
                mat.roughTexIdx = roughMap;
                mat.aoTexIdx = aoMap;
            }

            mat.heightmapScale = j.value("heightmapScale", 0.0);

            auto cullOffIt = j.find("cullOff");
            extraDat.noCull = cullOffIt != j.end();

            auto wireframeIt = j.find("wireframe");
            extraDat.wireframe = wireframeIt != j.end();
        } catch(nlohmann::detail::exception& ex) {
            std::string path = g_assetDB.getAssetPath(asset);
            logErr(WELogCategoryRender, "Invalid material document %s (%s)", path.c_str(), ex.what());
        }
    }

    uint32_t MaterialSlots::load(AssetID asset) {
        uint32_t slot = getFreeSlot();

        if (slot > NUM_MAT_SLOTS) {
            fatalErr("Out of material slots");
        }

        present[slot] = true;
        parseMaterial(asset, slots[slot], matExtraData[slot]);

        lookup.insert({ asset, slot });
        reverseLookup.insert({ slot, asset });

        return slot;
    }
}
