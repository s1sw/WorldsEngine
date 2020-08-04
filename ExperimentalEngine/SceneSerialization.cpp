#include "SceneSerialization.hpp"
#include "AssetDB.hpp"
#include "Engine.hpp"
#include "Transform.hpp"

const unsigned char SCN_FORMAT_ID = 0;
const unsigned char SCN_FORMAT_MAGIC[4] = { 'E','S','C','N' };

#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))

void saveScene(AssetID id, entt::registry& reg) {
    PHYSFS_File* file = g_assetDB.openAssetFileWrite(id);

    PHYSFS_writeBytes(file, SCN_FORMAT_MAGIC, 4);
    PHYSFS_writeBytes(file, &SCN_FORMAT_ID, 1);

    uint32_t numEnts = (uint32_t)reg.size();
    PHYSFS_writeBytes(file, &numEnts, sizeof(numEnts));

    reg.each([file, &reg](entt::entity ent) {
        PHYSFS_writeBytes(file, &ent, sizeof(ent));
        unsigned char compBitfield = 0;

        // This should always be true
        compBitfield |= reg.has<Transform>(ent) << 0;
        compBitfield |= reg.has<WorldObject>(ent) << 1;
        compBitfield |= reg.has<WorldLight>(ent) << 2;

        PHYSFS_writeBytes(file, &compBitfield, sizeof(compBitfield));

        if (reg.has<Transform>(ent)) {
            Transform& t = reg.get<Transform>(ent);
            PHYSFS_writeBytes(file, &t.position, sizeof(t.position));
            PHYSFS_writeBytes(file, &t.rotation, sizeof(t.rotation));
            PHYSFS_writeBytes(file, &t.scale, sizeof(t.scale));
        }

        if (reg.has<WorldObject>(ent)) {
            WorldObject& wObj = reg.get<WorldObject>(ent);
            PHYSFS_writeBytes(file, &wObj.material, sizeof(wObj.material));
            PHYSFS_writeBytes(file, &wObj.materialIndex, sizeof(wObj.materialIndex));
            PHYSFS_writeBytes(file, &wObj.mesh, sizeof(wObj.mesh));
            PHYSFS_writeBytes(file, &wObj.texScaleOffset, sizeof(wObj.texScaleOffset));
        }

        if (reg.has<WorldLight>(ent)) {
            WorldLight& wLight = reg.get<WorldLight>(ent);
            WRITE_FIELD(file, wLight.type);
            WRITE_FIELD(file, wLight.color);
            WRITE_FIELD(file, wLight.spotCutoff);
        }
        });
    PHYSFS_close(file);
}

void loadScene(AssetID id, entt::registry& reg) {
    PHYSFS_File* file = g_assetDB.openAssetFileRead(id);

    char magicCheck[5];
    magicCheck[4] = 0;
    PHYSFS_readBytes(file, magicCheck, 4);
    unsigned char formatId;
    PHYSFS_readBytes(file, &formatId, sizeof(formatId));

    if (formatId != SCN_FORMAT_ID) {
        std::cerr << "scene has wrong format id: got " << formatId << ", expected " << SCN_FORMAT_ID << "\n";
        return;
    }

    if (memcmp(magicCheck, SCN_FORMAT_MAGIC, 4) != 0) {
        std::cerr << "failed magic check: got " << magicCheck << ", expected " << SCN_FORMAT_MAGIC << "\n";
        return;
    }

    uint32_t numEntities;
    PHYSFS_readULE32(file, &numEntities);

    for (uint32_t i = 0; i < numEntities; i++) {
        uint32_t oldEntId;
        PHYSFS_readULE32(file, &oldEntId);

        auto newEnt = reg.create();

        unsigned char compBitfield = 0;
        PHYSFS_readBytes(file, &compBitfield, sizeof(compBitfield));

        if ((compBitfield & 1) == 1) {
            Transform& t = reg.emplace<Transform>(newEnt);

            PHYSFS_readBytes(file, &t.position, sizeof(t.position));
            PHYSFS_readBytes(file, &t.rotation, sizeof(t.rotation));
            PHYSFS_readBytes(file, &t.scale, sizeof(t.scale));
        }

        if ((compBitfield & 2) == 2) {
            WorldObject& wo = reg.emplace<WorldObject>(newEnt, 0, 0);

            PHYSFS_readBytes(file, &wo.material, sizeof(wo.material));
            PHYSFS_readBytes(file, &wo.materialIndex, sizeof(wo.materialIndex));
            PHYSFS_readBytes(file, &wo.mesh, sizeof(wo.mesh));
            PHYSFS_readBytes(file, &wo.texScaleOffset, sizeof(wo.texScaleOffset));
        }

        if ((compBitfield & 4) == 4) {
            WorldLight& wl = reg.emplace<WorldLight>(newEnt);

            READ_FIELD(file, wl.type);
            READ_FIELD(file, wl.color);
            READ_FIELD(file, wl.spotCutoff);
        }
    }

    PHYSFS_close(file);
}