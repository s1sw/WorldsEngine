#include "ResourceSlots.hpp"
#include "Engine.hpp"
#include "tracy/Tracy.hpp"
#include <sajson.h>

namespace worlds {
    void MaterialSlots::parseMaterial(AssetID asset, PackedMaterial& mat) {
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

        auto albedoPath = root.get_object_value(albedoPathIdx).as_string();
        std::string normalMapPath;
        if (normalMapIdx != root.get_length())
            normalMapPath = root.get_object_value(normalMapIdx).as_string();
        float metallic = (float)root.get_object_value(metallicIdx).get_double_value();
        float roughness = (float)root.get_object_value(roughnessIdx).get_double_value();
        const auto& albedoColorArr = root.get_object_value(albedoColorIdx);

        glm::vec3 albedoColor{
            albedoColorArr.get_array_element(0).get_double_value(),
            albedoColorArr.get_array_element(1).get_double_value(),
            albedoColorArr.get_array_element(2).get_double_value()
        };

        auto albedoAssetId = g_assetDB.addOrGetExisting(albedoPath);

        float nMapSlot = -1.0f;
        if (!normalMapPath.empty()) {
            auto normalMapId = g_assetDB.addOrGetExisting(normalMapPath);
            uint32_t nMapSlotU = texSlots.loadOrGet(normalMapId);
            nMapSlot = nMapSlotU;
        }

        mat.pack0 = glm::vec4(metallic, roughness, texSlots.loadOrGet(albedoAssetId), nMapSlot);
        mat.pack1 = glm::vec4(albedoColor, 0.0f);

        std::free(buffer);
    }

    uint32_t MaterialSlots::load(AssetID asset) {
        uint32_t slot = getFreeSlot();

        if (slot > NUM_MAT_SLOTS) {
            fatalErr("Out of material slots");
        }

        present[slot] = true;
        parseMaterial(asset, slots[slot]);

        lookup.insert({ asset, slot });
        reverseLookup.insert({ slot, asset });

        return slot;
    }
}