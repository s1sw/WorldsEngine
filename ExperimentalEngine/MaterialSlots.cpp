#include "ResourceSlots.hpp"
#include "Engine.hpp"
#include "tracy/Tracy.hpp"
#include <sajson.h>

namespace worlds {
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
        auto normalMapIdx = root.find_object_key(sajson::string("normalMapPath", 13));
        auto alphaCutoffIdx = root.find_object_key(sajson::string("alphaCutoff", 11));
        auto heightmapScaleIdx = root.find_object_key(sajson::string("heightmapScale", 14));
        auto heightmapPathIdx = root.find_object_key(sajson::string("heightmapPath", 13));
        auto emissiveColorIdx = root.find_object_key(sajson::string("emissiveColor", 13));

        auto albedoPath = root.get_object_value(albedoPathIdx).as_string();

        std::string normalMapPath;
        if (normalMapIdx != rootLength)
            normalMapPath = root.get_object_value(normalMapIdx).as_string();

        std::string heightmapPath;
        if (heightmapPathIdx != rootLength)
            heightmapPath = root.get_object_value(heightmapPathIdx).as_string();

        float alphaCutoff = 0.0f;
        if (alphaCutoffIdx != rootLength)
            alphaCutoff = root.get_object_value(alphaCutoffIdx).get_double_value();
        mat.alphaCutoff = alphaCutoff;

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
        }

        auto albedoAssetId = g_assetDB.addOrGetExisting(albedoPath);

        int nMapSlot = -1;
        if (!normalMapPath.empty()) {
            auto normalMapId = g_assetDB.addOrGetExisting(normalMapPath);
            nMapSlot = texSlots.loadOrGet(normalMapId);
        }

        int hMapSlot = -1;
        if (!heightmapPath.empty()) {
            auto heightMapId = g_assetDB.addOrGetExisting(heightmapPath);
            hMapSlot = texSlots.loadOrGet(heightMapId);
        }

        mat.albedoTexIdx = texSlots.loadOrGet(albedoAssetId);
        mat.normalTexIdx = nMapSlot;
        mat.heightmapTexIdx = hMapSlot;

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
