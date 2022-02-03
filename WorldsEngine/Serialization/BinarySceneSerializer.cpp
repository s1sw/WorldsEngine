#include "SceneSerialization.hpp"
#include "SceneSerializationFuncs.hpp"
#include "../Core/Log.hpp"
#include "../Core/Transform.hpp"
#include "../Util/TimingUtil.hpp"
#include "../ComponentMeta/ComponentMetadata.hpp"
#include "../Core/AssetDB.hpp"

namespace worlds {
    typedef void(*LoadSceneFunc)(PHYSFS_File*, entt::registry&, bool);

    const LoadSceneFunc idFuncs[] = {nullptr, loadScene01, loadScene02, loadScene03, loadScene04};

    const unsigned char ESCN_FORMAT_MAGIC[5] = { 'E','S','C','N', '\0' };
    const unsigned char WSCN_FORMAT_MAGIC[5] = "WSCN";
    const int MAX_ESCN_FORMAT_ID = 4;
    const int MAX_WSCN_FORMAT_ID = 6;


    void BinarySceneSerializer::saveScene(PHYSFS_File* file, entt::registry& reg) {
        PerfTimer timer;

        PHYSFS_writeBytes(file, WSCN_FORMAT_MAGIC, 4);
        PHYSFS_writeBytes(file, &MAX_WSCN_FORMAT_ID, 1);

        uint32_t numEnts = (uint32_t)reg.view<Transform>().size();
        PHYSFS_writeULE32(file, numEnts);

        reg.view<Transform>().each([file, &reg](entt::entity ent, Transform&) {
            PHYSFS_writeULE32(file, (uint32_t)ent);

            uint8_t numComponents = 0;

            for (auto& mdata : ComponentMetadataManager::sorted) {
                std::array<ENTT_ID_TYPE, 1> arr = { mdata->getComponentID() };
                auto rView = reg.runtime_view(arr.begin(), arr.end());

                if (!rView.contains(ent)) continue;
                numComponents++;
            }

            PHYSFS_writeBytes(file, &numComponents, 1);

            for (auto& mdata : ComponentMetadataManager::sorted) {
                std::array<ENTT_ID_TYPE, 1> arr = { mdata->getComponentID() };
                auto rView = reg.runtime_view(arr.begin(), arr.end());

                if (!rView.contains(ent)) continue;

                PHYSFS_writeULE32(file, mdata->getSerializedID());
                mdata->writeToFile(ent, reg, file);
            }
        });

        PHYSFS_close(file);

        logMsg("Saved scene in %.3fms", timer.stopGetMs());

        AssetDB::save();
    }

    // Old scene format before the renaming of the engine and restructure of serialization
    bool deserializeEScene(PHYSFS_File* file, entt::registry& reg, char* magicCheck, unsigned char formatId) {
        if (formatId > MAX_ESCN_FORMAT_ID) {
            logErr(WELogCategoryEngine, "scene has incompatible format id: got %i, expected %i or lower", formatId, MAX_ESCN_FORMAT_ID);
            return false;
        }

        if (memcmp(magicCheck, ESCN_FORMAT_MAGIC, 4) != 0) {
            logErr(WELogCategoryEngine, "failed magic check: got %s, expected %s", magicCheck, ESCN_FORMAT_MAGIC);
            return false;
        }

        logMsg("Loading experimental scene version %i", formatId);

        idFuncs[formatId](file, reg, true);
        return true;
    }

    bool deserializeWScene(PHYSFS_File* file, entt::registry& reg, char* magicCheck, unsigned char formatId) {
        PerfTimer timer;

        if (memcmp(magicCheck, WSCN_FORMAT_MAGIC, 4) != 0) {
            logErr(WELogCategoryEngine, "failed magic check: got %s, expected %s", magicCheck, WSCN_FORMAT_MAGIC);
            return false;
        }

        if (formatId > MAX_WSCN_FORMAT_ID) {
            logErr(WELogCategoryEngine, "Tried to load a wscn that is too new (%i, while we can load up to %i)",
                formatId, MAX_WSCN_FORMAT_ID);
            return false;
        }

        logMsg("Loading WSCN version %i", formatId);

        uint32_t numEntities;
        PHYSFS_readULE32(file, &numEntities);

        for (uint32_t i = 0; i < numEntities; i++) {
            uint32_t oldEntId;
            PHYSFS_readULE32(file, &oldEntId);

            auto newEnt = reg.create((entt::entity)oldEntId);

            uint8_t numComponents;
            PHYSFS_readBytes(file, &numComponents, 1);

            for (uint8_t j = 0; j < numComponents; j++) {
                uint32_t compType = ~0u;
                PHYSFS_readULE32(file, &compType);

                auto* mdata = ComponentMetadataManager::bySerializedID.at(compType);
                mdata->readFromFile(newEnt, reg, file, formatId);
            }
        }

        logMsg("loaded WSCN in %.3fms", timer.stopGetMs());
        return true;
    }

    void BinarySceneSerializer::loadScene(PHYSFS_File* file, entt::registry& reg) {
        char magicCheck[5];
        magicCheck[4] = 0; // Null byte just in case
        PHYSFS_readBytes(file, magicCheck, 4);

        unsigned char formatId;
        PHYSFS_readBytes(file, &formatId, sizeof(formatId));

        if (magicCheck[0] == 'W') {
            deserializeWScene(file, reg, magicCheck, formatId);
        } else if (magicCheck[0] == 'E') {
            deserializeEScene(file, reg, magicCheck, formatId);
        }
    }

    void BinarySceneSerializer::saveEntity(PHYSFS_File* file, entt::registry& reg, entt::entity ent) {
        assert(false && "Not implemented yet!");
    }

    entt::entity BinarySceneSerializer::loadEntity(PHYSFS_File* file, entt::registry& reg) {
        assert(false && "Not implemented yet!");
        return entt::null;
    }
}
