#include "ResourceSlots.hpp"
#include "../Core/Engine.hpp"
#include "tracy/Tracy.hpp"
#include <physfs.h>
#include <optional>
#include "../Util/JsonUtil.hpp"
#include <nlohmann/json.hpp>
#include <mutex>

namespace worlds {
    uint32_t MaterialSlots::getTexture(nlohmann::json& j, std::string key) {
        auto it = j.find(key);

        if (it == j.end()) return ~0u;

        std::string path = it.value();

        return texSlots.loadOrGet(AssetDB::pathToId(path));
    }

    void MaterialSlots::parseMaterial(AssetID asset, PackedMaterial& mat, MatExtraData& extraDat) {
        ZoneScoped;
        PHYSFS_File* f = AssetDB::openAssetFileRead(asset);

        if (f == nullptr) {
            std::string path = AssetDB::idToPath(asset);
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

            std::string albedoPath = j["albedoPath"];

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

            auto albedoAssetId = AssetDB::pathToId(albedoPath);

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
            std::string path = AssetDB::idToPath(asset);
            logErr(WELogCategoryRender, "Invalid material document %s (%s)", path.c_str(), ex.what());
        }
    }

    uint32_t MaterialSlots::load(AssetID asset) {
        slotMutex->lock();
        uint32_t slot = getFreeSlot();

        if (slot > NUM_MAT_SLOTS) {
            fatalErr("Out of material slots");
        }

        lookup.insert({ asset, slot });
        reverseLookup.insert({ slot, asset });
        present[slot] = true;
        slotMutex->unlock();

        parseMaterial(asset, slots[slot], matExtraData[slot]);


        return slot;
    }

    MatExtraData& MaterialSlots::getExtraDat(uint32_t slot) {
        assert(present[slot]);
        return matExtraData[slot];
    }

    MaterialSlots::MaterialSlots(std::shared_ptr<VulkanHandles> vkCtx, TextureSlots& texSlots)
        : vkCtx(vkCtx)
        , texSlots(texSlots) {
        slotMutex = new std::mutex;
    }

    void MaterialSlots::unload(int idx) {
        present[idx] = false;
        lookup.erase(reverseLookup.at(idx));
        reverseLookup.erase(idx);
    }

    MaterialSlots::~MaterialSlots() {
        delete slotMutex;
    }
}
