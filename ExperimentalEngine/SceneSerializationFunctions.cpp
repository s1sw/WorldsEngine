#include "SceneSerialization.hpp"
#include "AssetDB.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include "PhysicsActor.hpp"
#include "Physics.hpp"
#include <filesystem>
#include "Log.hpp"
#include "TimingUtil.hpp"
#include "SceneSerializationFuncs.hpp"
#include "NameComponent.hpp"

namespace worlds {
    extern SceneInfo currentScene;

    const unsigned char LATEST_SCN_FORMAT_ID = 3;
    const unsigned char SCN_FORMAT_MAGIC[5] = { 'E','S','C','N', '\0' };

#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))

    void saveScene(AssetID id, entt::registry& reg) {
        PerfTimer timer;
        PHYSFS_File* file = g_assetDB.openAssetFileWrite(id);

        PHYSFS_writeBytes(file, SCN_FORMAT_MAGIC, 4);
        PHYSFS_writeBytes(file, &LATEST_SCN_FORMAT_ID, 1);

        uint32_t numEnts = (uint32_t)reg.view<Transform>().size();
        PHYSFS_writeBytes(file, &numEnts, sizeof(numEnts));

        reg.view<Transform>().each([file, &reg](entt::entity ent, Transform& t) {
            PHYSFS_writeBytes(file, &ent, sizeof(ent));
            unsigned char compBitfield = 0;

            // This will always be true
            compBitfield |= reg.has<Transform>(ent) << 0;
            compBitfield |= reg.has<WorldObject>(ent) << 1;
            compBitfield |= reg.has<WorldLight>(ent) << 2;
            compBitfield |= reg.has<PhysicsActor>(ent) << 3;
            compBitfield |= reg.has<DynamicPhysicsActor>(ent) << 4;
            compBitfield |= reg.has<NameComponent>(ent) << 5;

            PHYSFS_writeBytes(file, &compBitfield, sizeof(compBitfield));

            if (reg.has<Transform>(ent)) {
                Transform& t = reg.get<Transform>(ent);
                PHYSFS_writeBytes(file, &t.position, sizeof(t.position));
                PHYSFS_writeBytes(file, &t.rotation, sizeof(t.rotation));
                PHYSFS_writeBytes(file, &t.scale, sizeof(t.scale));
            }

            if (reg.has<WorldObject>(ent)) {
                WorldObject& wObj = reg.get<WorldObject>(ent);
                for (int j = 0; j < NUM_SUBMESH_MATS; j++) {
                    bool isPresent = wObj.presentMaterials[j];
                    WRITE_FIELD(file, isPresent);

                    if (isPresent) {
                        AssetID matId = wObj.materials[j];
                        WRITE_FIELD(file, matId);
                    }
                }
                PHYSFS_writeBytes(file, &wObj.mesh, sizeof(wObj.mesh));
                PHYSFS_writeBytes(file, &wObj.texScaleOffset, sizeof(wObj.texScaleOffset));
            }

            if (reg.has<WorldLight>(ent)) {
                WorldLight& wLight = reg.get<WorldLight>(ent);
                WRITE_FIELD(file, wLight.type);
                WRITE_FIELD(file, wLight.color);
                WRITE_FIELD(file, wLight.spotCutoff);
            }

            if (reg.has<PhysicsActor>(ent)) {
                PhysicsActor& pa = reg.get<PhysicsActor>(ent);
                PHYSFS_writeULE16(file, (uint16_t)pa.physicsShapes.size());

                for (size_t i = 0; i < pa.physicsShapes.size(); i++) {
                    auto shape = pa.physicsShapes[i];
                    WRITE_FIELD(file, shape.type);

                    WRITE_FIELD(file, shape.pos);
                    WRITE_FIELD(file, shape.rot);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        WRITE_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        WRITE_FIELD(file, shape.box.halfExtents.x);
                        WRITE_FIELD(file, shape.box.halfExtents.y);
                        WRITE_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        WRITE_FIELD(file, shape.capsule.height);
                        WRITE_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }
            }

            if (reg.has<DynamicPhysicsActor>(ent)) {
                DynamicPhysicsActor& pa = reg.get<DynamicPhysicsActor>(ent);
                WRITE_FIELD(file, pa.mass);
                PHYSFS_writeULE16(file, (uint16_t)pa.physicsShapes.size());

                for (size_t i = 0; i < pa.physicsShapes.size(); i++) {
                    auto shape = pa.physicsShapes[i];
                    WRITE_FIELD(file, shape.type);

                    WRITE_FIELD(file, shape.pos);
                    WRITE_FIELD(file, shape.rot);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        WRITE_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        WRITE_FIELD(file, shape.box.halfExtents.x);
                        WRITE_FIELD(file, shape.box.halfExtents.y);
                        WRITE_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        WRITE_FIELD(file, shape.capsule.height);
                        WRITE_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }
            }

            if (reg.has<NameComponent>(ent)) {
                NameComponent& nc = reg.get<NameComponent>(ent);

                int nameLen = nc.name.length();
                WRITE_FIELD(file, nameLen);

                PHYSFS_writeBytes(file, nc.name.data(), nameLen);
            }
        });
        PHYSFS_close(file);

        currentScene.id = id;
        currentScene.name = std::filesystem::path(g_assetDB.getAssetPath(id)).stem().string();
        logMsg("Saved scene in %.3fms", timer.stopGetMs());

        g_assetDB.save();
    }

    void loadScene01(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive) {
        PerfTimer timer;
        

        uint32_t numEntities;
        PHYSFS_readULE32(file, &numEntities);

        if (!additive)
            reg.clear();

        for (uint32_t i = 0; i < numEntities; i++) {
            uint32_t oldEntId;
            PHYSFS_readULE32(file, &oldEntId);

            auto newEnt = reg.create((entt::entity)oldEntId);

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

                PHYSFS_readBytes(file, &wo.materials[0], sizeof(wo.materials[0]));
                PHYSFS_readBytes(file, &wo.mesh, sizeof(wo.mesh));
                PHYSFS_readBytes(file, &wo.texScaleOffset, sizeof(wo.texScaleOffset));
            }

            if ((compBitfield & 4) == 4) {
                WorldLight& wl = reg.emplace<WorldLight>(newEnt);

                READ_FIELD(file, wl.type);
                READ_FIELD(file, wl.color);
                READ_FIELD(file, wl.spotCutoff);
            }

            if ((compBitfield & 8) == 8) {
                auto* pActor = g_physics->createRigidStatic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);

                PhysicsActor& pa = reg.emplace<PhysicsActor>(newEnt, pActor);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }

            if ((compBitfield & 16) == 16) {
                auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);
                DynamicPhysicsActor& pa = reg.emplace<DynamicPhysicsActor>(newEnt, pActor);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }
        }

        currentScene.name = std::filesystem::path(g_assetDB.getAssetPath(id)).stem().string();
        currentScene.id = id;
        logMsg("Loaded scene in %.3fms", timer.stopGetMs());
    }

    void loadScene02(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive) {
        PerfTimer timer;

        uint32_t numEntities;
        PHYSFS_readULE32(file, &numEntities);

        if (!additive)
            reg.clear();

        for (uint32_t i = 0; i < numEntities; i++) {
            uint32_t oldEntId;
            PHYSFS_readULE32(file, &oldEntId);

            auto newEnt = reg.create((entt::entity)oldEntId);

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

                for (int j = 0; j < NUM_SUBMESH_MATS; j++) {
                    bool isPresent;
                    PHYSFS_readBytes(file, &isPresent, sizeof(isPresent));
                    wo.presentMaterials[j] = isPresent;

                    AssetID mat;
                    if (isPresent) {
                        PHYSFS_readBytes(file, &mat, sizeof(mat));
                        wo.materials[j] = mat;
                        wo.materialIdx[j] = ~0u;
                    }
                }

                PHYSFS_readBytes(file, &wo.mesh, sizeof(wo.mesh));
                PHYSFS_readBytes(file, &wo.texScaleOffset, sizeof(wo.texScaleOffset));
            }

            if ((compBitfield & 4) == 4) {
                WorldLight& wl = reg.emplace<WorldLight>(newEnt);

                READ_FIELD(file, wl.type);
                READ_FIELD(file, wl.color);
                READ_FIELD(file, wl.spotCutoff);
            }

            if ((compBitfield & 8) == 8) {
                auto* pActor = g_physics->createRigidStatic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);

                PhysicsActor& pa = reg.emplace<PhysicsActor>(newEnt, pActor);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }

            if ((compBitfield & 16) == 16) {
                auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);
                DynamicPhysicsActor& pa = reg.emplace<DynamicPhysicsActor>(newEnt, pActor);

                READ_FIELD(file, pa.mass);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
                updateMass(pa);
            }
        }

        currentScene.name = std::filesystem::path(g_assetDB.getAssetPath(id)).stem().string();
        currentScene.id = id;
        logMsg("Loaded scene in %.3fms", timer.stopGetMs());
    }

    void loadScene03(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive) {
        PerfTimer timer;

        uint32_t numEntities;
        PHYSFS_readULE32(file, &numEntities);

        if (!additive)
            reg.clear();

        for (uint32_t i = 0; i < numEntities; i++) {
            uint32_t oldEntId;
            PHYSFS_readULE32(file, &oldEntId);

            auto newEnt = reg.create((entt::entity)oldEntId);

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

                for (int j = 0; j < NUM_SUBMESH_MATS; j++) {
                    bool isPresent;
                    PHYSFS_readBytes(file, &isPresent, sizeof(isPresent));
                    wo.presentMaterials[j] = isPresent;

                    AssetID mat;
                    if (isPresent) {
                        PHYSFS_readBytes(file, &mat, sizeof(mat));
                        wo.materials[j] = mat;
                        wo.materialIdx[j] = ~0u;
                    }
                }

                PHYSFS_readBytes(file, &wo.mesh, sizeof(wo.mesh));
                PHYSFS_readBytes(file, &wo.texScaleOffset, sizeof(wo.texScaleOffset));
            }

            if ((compBitfield & 4) == 4) {
                WorldLight& wl = reg.emplace<WorldLight>(newEnt);

                READ_FIELD(file, wl.type);
                READ_FIELD(file, wl.color);
                READ_FIELD(file, wl.spotCutoff);
            }

            if ((compBitfield & 8) == 8) {
                auto* pActor = g_physics->createRigidStatic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);

                PhysicsActor& pa = reg.emplace<PhysicsActor>(newEnt, pActor);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    READ_FIELD(file, shape.pos);
                    READ_FIELD(file, shape.rot);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }

            if ((compBitfield & 16) == 16) {
                auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(newEnt)));
                g_scene->addActor(*pActor);
                DynamicPhysicsActor& pa = reg.emplace<DynamicPhysicsActor>(newEnt, pActor);

                READ_FIELD(file, pa.mass);

                uint16_t shapeCount;
                PHYSFS_readULE16(file, &shapeCount);

                pa.physicsShapes.resize(shapeCount);

                for (uint16_t i = 0; i < shapeCount; i++) {
                    auto& shape = pa.physicsShapes[i];
                    READ_FIELD(file, shape.type);

                    READ_FIELD(file, shape.pos);
                    READ_FIELD(file, shape.rot);

                    switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        READ_FIELD(file, shape.sphere.radius);
                        break;
                    case PhysicsShapeType::Box:
                        READ_FIELD(file, shape.box.halfExtents.x);
                        READ_FIELD(file, shape.box.halfExtents.y);
                        READ_FIELD(file, shape.box.halfExtents.z);
                        break;
                    case PhysicsShapeType::Capsule:
                        READ_FIELD(file, shape.capsule.height);
                        READ_FIELD(file, shape.capsule.radius);
                        break;
                    }
                }

                updatePhysicsShapes(pa);
                updateMass(pa);
            }

            if ((compBitfield & 32) == 32) {
                NameComponent& nc = reg.emplace<NameComponent>(newEnt);
                int nameLen;
                READ_FIELD(file, nameLen);

                nc.name.resize(nameLen);
                PHYSFS_readBytes(file, nc.name.data(), nameLen);
            }
        }

        currentScene.name = std::filesystem::path(g_assetDB.getAssetPath(id)).stem().string();
        currentScene.id = id;
        logMsg("Loaded scene in %.3fms", timer.stopGetMs());
    }
}