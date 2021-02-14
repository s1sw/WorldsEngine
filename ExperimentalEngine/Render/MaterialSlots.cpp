#include "ResourceSlots.hpp"
#include "Engine.hpp"
#include "tracy/Tracy.hpp"
#include <sajson.h>
#include <optional>
#include "JsonUtil.hpp"

namespace worlds {
    std::optional<std::string> getString(const sajson::value& obj, const char* key) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return std::nullopt;

        return obj.get_object_value(idx).as_string();
    }

    void MaterialSlots::parseMaterial(AssetID asset, PackedMaterial& mat, MatExtraData& extraDat) {
        ZoneScoped;
        PHYSFS_File* f = g_assetDB.openAssetFileRead(asset);
        size_t fileSize = PHYSFS_fileLength(f);
        char* buffer = (char*)std::malloc(fileSize);
        PHYSFS_readBytes(f, buffer, fileSize);
        PHYSFS_close(f);

        const sajson::document& document = sajson::parse(
            sajson::single_allocation(), sajson::mutable_string_view(fileSize, buffer)
        );

        if (!document.is_valid()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid material document");
            std::free(buffer);
            return;
        }

        const auto& root = document.get_root();

        if (root.get_type() != sajson::TYPE_OBJECT) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid material document");
            std::free(buffer);
            return;
        }

        auto rootLength = root.get_length();
        auto albedoPathIdx = root.find_object_key(sajson::string("albedoPath", 10));
        auto metallicIdx = root.find_object_key(sajson::string("metallic", 8));
        auto roughnessIdx = root.find_object_key(sajson::string("roughness", 9));
        auto albedoColorIdx = root.find_object_key(sajson::string("albedoColor", 11));
        auto alphaCutoffIdx = root.find_object_key(sajson::string("alphaCutoff", 11));
        auto heightmapScaleIdx = root.find_object_key(sajson::string("heightmapScale", 14));
        auto emissiveColorIdx = root.find_object_key(sajson::string("emissiveColor", 13));

        auto albedoPath = root.get_object_value(albedoPathIdx).as_string();

        float alphaCutoff = 0.0f;
        if (alphaCutoffIdx != rootLength)
            alphaCutoff = root.get_object_value(alphaCutoffIdx).get_double_value();

        mat.setCutoff(alphaCutoff);
        mat.setFlags(MaterialFlags::None);
        logMsg("set %.3f, got %.3f", alphaCutoff, mat.getCutoff());

        mat.metallic = (float)root.get_object_value(metallicIdx).get_double_value();
        mat.roughness = (float)root.get_object_value(roughnessIdx).get_double_value();
        const auto& albedoColorArr = root.get_object_value(albedoColorIdx);

        glm::vec3 albedoColor{
            albedoColorArr.get_array_element(0).get_double_value(),
            albedoColorArr.get_array_element(1).get_double_value(),
            albedoColorArr.get_array_element(2).get_double_value()
        };

        mat.albedoColor = albedoColor;

        if (emissiveColorIdx != rootLength) {
            const auto& emissiveColorArr = root.get_object_value(emissiveColorIdx);

            mat.emissiveColor = glm::vec3 {
               emissiveColorArr.get_array_element(0).get_double_value(), 
               emissiveColorArr.get_array_element(1).get_double_value(), 
               emissiveColorArr.get_array_element(2).get_double_value()
            };
        } else {
            mat.emissiveColor = glm::vec3 {0.0f};
        }

        auto albedoAssetId = g_assetDB.addOrGetExisting(albedoPath);

        uint32_t nMapSlot = ~0u;
        auto normalMap = getString(root, "normalMapPath");
        if (normalMap) {
            auto normalMapId = g_assetDB.addOrGetExisting(*normalMap);
            nMapSlot = texSlots.loadOrGet(normalMapId);
        }

        uint32_t hMapSlot = ~0u;
        auto heightmap = getString(root, "heightmapPath");
        if (heightmap) {
            auto heightMapId = g_assetDB.addOrGetExisting(*heightmap);
            hMapSlot = texSlots.loadOrGet(heightMapId);
        }

        mat.albedoTexIdx = texSlots.loadOrGet(albedoAssetId);
        mat.normalTexIdx = nMapSlot;
        mat.heightmapTexIdx = hMapSlot;

        mat.metalTexIdx = ~0u;
        mat.roughTexIdx = ~0u;
        mat.aoTexIdx = ~0u;

        auto metalMap = getString(root, "metalMapPath");
        auto roughMap = getString(root, "roughMapPath");
        auto aoMap = getString(root, "aoMapPath");
        auto pbrMap = getString(root, "pbrMapPath");

        if (metalMap)
            mat.metalTexIdx = texSlots.loadOrGet(g_assetDB.addOrGetExisting(*metalMap));

        if (roughMap)
            mat.roughTexIdx = texSlots.loadOrGet(g_assetDB.addOrGetExisting(*roughMap));

        if (aoMap)
            mat.aoTexIdx = texSlots.loadOrGet(g_assetDB.addOrGetExisting(*aoMap));

        if (pbrMap) {
            mat.roughTexIdx = texSlots.loadOrGet(g_assetDB.addOrGetExisting(*pbrMap));
            mat.setFlags(mat.getFlags() | MaterialFlags::UsePackedPBR);
        }

        float heightmapScale = 0.0f;
        if (heightmapScaleIdx != rootLength)
            heightmapScale = root.get_object_value(heightmapScaleIdx).get_double_value();
        mat.heightmapScale = heightmapScale;
        
        auto bfCullOffIdx = root.find_object_key(sajson::string("cullOff", 7));
        extraDat.noCull = bfCullOffIdx != rootLength;

        auto wireframeIdx = root.find_object_key(sajson::string("wireframe", 9));
        extraDat.wireframe = wireframeIdx != rootLength;

        std::free(buffer);
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
