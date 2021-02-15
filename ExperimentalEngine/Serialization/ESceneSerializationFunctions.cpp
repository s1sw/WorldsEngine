#include "SceneSerialization.hpp"
#include "../Core/AssetDB.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include "../Physics/PhysicsActor.hpp"
#include "../Physics/Physics.hpp"
#include <filesystem>
#include "../Core/Log.hpp"
#include "../Util/TimingUtil.hpp"
#include "SceneSerializationFuncs.hpp"
#include "../Core/NameComponent.hpp"
#include "../Render/Render.hpp"
#include "../ComponentMeta/ComponentMetadata.hpp"

namespace worlds {
    const unsigned char LATEST_SCN_FORMAT_ID = 1;
    const unsigned char SCN_FORMAT_MAGIC[5] = { 'W','S','C','N', '\0' };

#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))


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
                    default:
                        logErr("invalid physics shape type??");
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
                    default:
                        logErr("invalid physics shape type??");
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }
        }

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
                    default:
                        logErr("invalid physics shape type??");
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
                    default:
                        logErr("invalid physics shape type??");
                        break;
                    }
                }

                updatePhysicsShapes(pa);
                updateMass(pa);
            }
        }

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
                    default:
                        logErr("invalid physics shape type??");
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }

            if ((compBitfield & 16) == 16) {
                auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(newEnt)));
                pActor->setSolverIterationCounts(12, 4);
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
                    default:
                        logErr("invalid physics shape type??");
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

        logMsg("Loaded scene in %.3fms", timer.stopGetMs());
    }

    void loadScene04(AssetID id, PHYSFS_File* file, entt::registry& reg, bool additive) {
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
                    default:
                        logErr("invalid physics shape type??");
                        break;
                    }
                }

                updatePhysicsShapes(pa);
            }

            if ((compBitfield & 16) == 16) {
                auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(newEnt)));
                pActor->setSolverIterationCounts(12, 4);
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
                    default:
                        logErr("invalid physics shape type??");
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

            if ((compBitfield & 64) == 64) {
                WorldCubemap& wc = reg.emplace<WorldCubemap>(newEnt);
                READ_FIELD(file, wc.cubemapId);
                READ_FIELD(file, wc.extent);
            }
        }

        logMsg("Loaded scene in %.3fms", timer.stopGetMs());
    }
}
