#include "Core/IGameEventHandler.hpp"
#include "Scripting/NetVM.hpp"
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>
#include "PhysicsActor.hpp"
#include <Core/Console.hpp>
#include <ImGui/imgui.h>
#include <SDL_cpuinfo.h>
#include <Core/Fatal.hpp>
#include "Physics.hpp"
#include "D6Joint.hpp"
#include "FixedJoint.hpp"
#include <physx/PxSceneDesc.h>
#include <physx/PxSimulationEventCallback.h>
#include <slib/Intrinsic.hpp>
#include <entt/entity/registry.hpp>
#include <Util/MathsUtil.hpp>
using namespace physx;

#define ENABLE_PVD 0

namespace worlds {
    class PhysErrCallback : public physx::PxErrorCallback {
    public:
        virtual void reportError(physx::PxErrorCode::Enum code, const char* msg, const char* file, int line) {
            switch (code) {
                default:
                case physx::PxErrorCode::eDEBUG_INFO:
                    logVrb(WELogCategoryPhysics, "%s (%s:%i)", msg, file, line);
                    break;
                case PxErrorCode::eDEBUG_WARNING:
                case PxErrorCode::ePERF_WARNING:
                case PxErrorCode::eINVALID_OPERATION:
                case PxErrorCode::eINVALID_PARAMETER:
                    if (strcmp(msg, "PxScene::getRenderBuffer() not allowed while simulation is running.") == 0) return;
                    logWarn(WELogCategoryPhysics, "%s (%s:%i)", msg, file, line);
                    break;
                case PxErrorCode::eINTERNAL_ERROR:
                case PxErrorCode::eABORT:
                case PxErrorCode::eOUT_OF_MEMORY:
                    logErr(WELogCategoryPhysics, "%s (%s:%i)", msg, file, line);
                    break;
            }
        }
    };

    physx::PxMaterial* defaultMaterial;
    PhysErrCallback gErrorCallback;
    physx::PxDefaultAllocator gDefaultAllocator;
    physx::PxFoundation* g_physFoundation;
    physx::PxPhysics* g_physics;
#if ENABLE_PVD
    physx::PxPvd* g_pvd;
    physx::PxPvdTransport* g_pvdTransport;
#endif
    physx::PxCooking* g_cooking;
    physx::PxScene* g_scene;
    physx::PxRigidBody* dummyBody;

    bool started = false;

    void* entToPtr(entt::entity ent) {
        return (void*)(uintptr_t)(uint32_t)ent;
    }

    entt::entity ptrToEnt(void* ptr) {
        return (entt::entity)(uint32_t)(uintptr_t)ptr;
    }

    template <typename T>
    void destroyPhysXActor(entt::registry& reg, entt::entity ent) {
        auto& pa = reg.get<T>(ent);
        pa.actor->release();
    }

    template <typename T>
    void setPhysXActorUserdata(entt::registry& reg, entt::entity ent) {
        auto& pa = reg.get<T>(ent);
        pa.actor->userData = entToPtr(ent);
    }

    void setupD6Joint(entt::registry& reg, entt::entity ent) {
        auto& j = reg.get<D6Joint>(ent);
        auto* pxj = physx::PxD6JointCreate(*g_physics, dummyBody, physx::PxTransform{ physx::PxIdentity }, nullptr, physx::PxTransform{ physx::PxIdentity });

        j.pxJoint = pxj;

        if (!reg.has<DynamicPhysicsActor>(ent)) {
            logWarn("D6 joint added to entity without a dynamic physics actor");
            return;
        }

        auto& dpa = reg.get<DynamicPhysicsActor>(ent);
        j.thisActor = dpa.actor;
        j.originalThisActor = dpa.actor;
        j.updateJointActors();
    }

    void destroyD6Joint(entt::registry& reg, entt::entity ent) {
        auto& j = reg.get<D6Joint>(ent);
        if (j.pxJoint) {
            j.pxJoint->release();
        }
    }

    void setupFixedJoint(entt::registry& reg, entt::entity ent) {
        auto& j = reg.get<FixedJoint>(ent);

        if (!reg.has<DynamicPhysicsActor>(ent)) {
            logErr("Fixed joint added to entity without a dynamic physics actor");
            return;
        }

        auto& dpa = reg.get<DynamicPhysicsActor>(ent);
        j.pxJoint = physx::PxFixedJointCreate(*g_physics, dpa.actor, physx::PxTransform{ physx::PxIdentity }, nullptr, physx::PxTransform{ physx::PxIdentity });
        j.pxJoint->setInvMassScale0(1.0f);
        j.pxJoint->setInvMassScale1(1.0f);
        j.thisActor = dpa.actor;
    }

    void destroyFixedJoint(entt::registry& reg, entt::entity ent) {
        auto& j = reg.get<FixedJoint>(ent);
        if (j.pxJoint) {
            j.pxJoint->release();
        }
    }

    void cmdTogglePhysVis(void*, const char*) {
        float currentScale = g_scene->getVisualizationParameter(physx::PxVisualizationParameter::eSCALE);

        g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f - currentScale);
    }

    void cmdToggleShapeVis(void*, const char*) {
        float currentVal = g_scene->getVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES);

        g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f - currentVal);
    }

    void cmdToggleMassAxesVis(void*, const char*) {
        float currentVal = g_scene->getVisualizationParameter(physx::PxVisualizationParameter::eBODY_MASS_AXES);

        g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_MASS_AXES, 1.0f - currentVal);
    }

    robin_hood::unordered_map<AssetID, physx::PxTriangleMesh*> physicsTriMesh;

    template <typename T>
    void updatePhysicsShapes(T& pa, glm::vec3 scale) {
        uint32_t nShapes = pa.actor->getNbShapes();
        physx::PxShape** buf = (physx::PxShape**)std::malloc(nShapes * sizeof(physx::PxShape*));
        pa.actor->getShapes(buf, nShapes);

        for (uint32_t i = 0; i < nShapes; i++) {
            pa.actor->detachShape(*buf[i]);
        }

        if (!pa.scaleShapes) scale = glm::vec3{ 1.0f };

        std::free(buf);

        for (PhysicsShape& ps : pa.physicsShapes) {
            physx::PxShape* shape;
            physx::PxMaterial* mat = ps.material ? ps.material : defaultMaterial;

            switch (ps.type) {
            case PhysicsShapeType::Box:
                shape = g_physics->createShape(
                    physx::PxBoxGeometry(glm2px(ps.box.halfExtents * scale)),
                    *mat
                );
                break;
            default:
                ps.sphere.radius = 0.5f;
            case PhysicsShapeType::Sphere:
                shape = g_physics->createShape(
                    physx::PxSphereGeometry(ps.sphere.radius * glm::compAdd(scale) / 3.0f),
                    *mat
                );
                break;
            case PhysicsShapeType::Capsule:
                shape = g_physics->createShape(
                    physx::PxCapsuleGeometry(ps.capsule.radius, ps.capsule.height * 0.5f),
                    *mat
                );
                break;
            case PhysicsShapeType::Mesh:
                {
                    if (ps.mesh.mesh == ~0u) {
                        logErr(WELogCategoryPhysics, "Mesh collider is missing a mesh!");
                        continue;
                    }
                    if (!physicsTriMesh.contains(ps.mesh.mesh)) {
                        const LoadedMesh& lm = MeshManager::loadOrGet(ps.mesh.mesh);

                        std::vector<physx::PxVec3> points;
                        points.resize(lm.vertices.size());

                        for (size_t i = 0; i < lm.vertices.size(); i++) {
                            points[i] = glm2px(lm.vertices[i].position);
                        }

                        physx::PxTriangleMeshDesc meshDesc;

                        meshDesc.points.count = points.size();
                        meshDesc.points.stride = sizeof(physx::PxVec3);
                        meshDesc.points.data = points.data();

                        meshDesc.triangles.count = lm.indices.size() / 3;
                        meshDesc.triangles.data = lm.indices.data();
                        meshDesc.triangles.stride = sizeof(uint32_t) * 3;

                        physx::PxTriangleMesh* triMesh = g_cooking->createTriangleMesh(meshDesc, g_physics->getPhysicsInsertionCallback());
                        physicsTriMesh.insert({ ps.mesh.mesh, triMesh });
                    }

                    PxMeshScale meshScale{PxVec3{scale.x, scale.y, scale.z}, PxQuat{PxIdentity}};
                    shape = g_physics->createShape(
                        physx::PxTriangleMeshGeometry(physicsTriMesh.at(ps.mesh.mesh), meshScale),
                        *mat
                    );
                }
                break;
            case PhysicsShapeType::ConvexMesh:
                {
                    if (ps.convexMesh.mesh == ~0u) {
                        logErr(WELogCategoryPhysics, "Convex mesh collider is missing a mesh!");
                        continue;
                    }

                    const LoadedMesh& lm = MeshManager::loadOrGet(ps.convexMesh.mesh);

                    std::vector<physx::PxVec3> points;
                    points.resize(lm.vertices.size());

                    for (size_t i = 0; i < lm.vertices.size(); i++) {
                        points[i] = glm2px(lm.vertices[i].position * scale);
                    }

                    physx::PxConvexMeshDesc convexDesc;
                    convexDesc.points.count = points.size();
                    convexDesc.points.stride = sizeof(physx::PxVec3);
                    convexDesc.points.data = points.data();
                    convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

                    physx::PxDefaultMemoryOutputStream buf;
                    physx::PxConvexMeshCookingResult::Enum result;

                    if (!g_cooking->cookConvexMesh(convexDesc, buf, &result)) {
                        logErr(WELogCategoryPhysics, "Failed to cook mesh %s", AssetDB::idToPath(ps.convexMesh.mesh).c_str());
                        continue;
                    }

                    physx::PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
                    physx::PxConvexMesh* convexMesh = g_physics->createConvexMesh(input);

                    shape = g_physics->createShape(
                        physx::PxConvexMeshGeometry(convexMesh),
                        *mat
                    );
                }
                break;
            }

            shape->setLocalPose(physx::PxTransform{ glm2px(ps.pos * scale), glm2px(ps.rot) });
            physx::PxFilterData data;
            data.word0 = pa.layer;
            data.word1 = pa.useContactMod;
            shape->setSimulationFilterData(data);
            shape->setQueryFilterData(data);

            pa.actor->attachShape(*shape);
            shape->release();
        }
    }

    template void updatePhysicsShapes<PhysicsActor>(PhysicsActor& pa, glm::vec3 scale);
    template void updatePhysicsShapes<DynamicPhysicsActor>(DynamicPhysicsActor& pa, glm::vec3 scale);

    uint32_t physicsLayerMask[32] =
    {
        0xFFFFFFFF,
        0xFFFFFFFD,
        0x00000000,
        0xFFFFFFFF
    };

    static physx::PxFilterFlags filterShader(
        physx::PxFilterObjectAttributes attributes1,
        physx::PxFilterData data1,
        physx::PxFilterObjectAttributes attributes2,
        physx::PxFilterData data2,
        physx::PxPairFlags& pairFlags,
        const void*,
        physx::PxU32) {

        int layer1 = slib::Intrinsics::bitScanForward(data1.word0);
        int layer2 = slib::Intrinsics::bitScanForward(data2.word0);

        if ((physicsLayerMask[layer1] & data2.word0) == 0 ||
            (physicsLayerMask[layer2] & data1.word0) == 0)
            return physx::PxFilterFlag::eKILL;


        pairFlags = physx::PxPairFlag::eSOLVE_CONTACT
                  | physx::PxPairFlag::eDETECT_DISCRETE_CONTACT
                  | physx::PxPairFlag::eDETECT_CCD_CONTACT
                  | physx::PxPairFlag::eNOTIFY_TOUCH_FOUND
                  | physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;

        if (data1.word1 || data2.word1) {
            pairFlags |= PxPairFlag::eMODIFY_CONTACTS;
        }

        return physx::PxFilterFlags();
    }

    DotNetScriptEngine* physScriptEngine;

    class SimulationCallback : public PxSimulationEventCallback {
    public:
        SimulationCallback(entt::registry& reg) : reg{reg} {}

        void onConstraintBreak(PxConstraintInfo* constraints, uint32_t count) override {}

        void onWake(PxActor** actors, uint32_t count) override {}

        void onSleep(PxActor** actors, uint32_t count) override {}

        void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, uint32_t nbPairs) override {
            entt::entity a = ptrToEnt(pairHeader.actors[0]->userData);
            entt::entity b = ptrToEnt(pairHeader.actors[1]->userData);


            auto evtA = reg.try_get<PhysicsEvents>(a);
            auto evtB = reg.try_get<PhysicsEvents>(b);


            glm::vec3 velA{0.0f};
            glm::vec3 velB{0.0f};

            auto aDynamic = pairHeader.actors[0]->is<PxRigidDynamic>();
            auto bDynamic = pairHeader.actors[1]->is<PxRigidDynamic>();

            if (aDynamic) {
                velA = px2glm(aDynamic->getLinearVelocity());
            }

            if (bDynamic) {
                velB = px2glm(bDynamic->getLinearVelocity());
            }

            PhysicsContactInfo info {
                .relativeSpeed = glm::distance(velA, velB)
            };

            const uint32_t contactBufSize = 32;
            PxContactPairPoint contacts[contactBufSize];
            uint32_t totalContacts = 0;

            for (uint32_t i = 0; i < nbPairs; i++) {
                auto& pair = pairs[i];
                PxU32 nbContacts = pair.extractContacts(contacts, contactBufSize);

                for (uint32_t j = 0; j < nbContacts; j++) {
                    totalContacts++;
                    info.averageContactPoint += px2glm(contacts[j].position);
                    info.normal += px2glm(contacts[j].normal);
                }
            }

            info.averageContactPoint /= totalContacts;
            info.normal /= totalContacts;


            info.otherEntity = b;
            physScriptEngine->handleCollision(a, &info);

            info.otherEntity = a;
            physScriptEngine->handleCollision(b, &info);

            if (evtA == nullptr && evtB == nullptr) return;

            if (evtA) {
                info.otherEntity = b;

                for (int i = 0; i < 4; i++) {
                    if (evtA->onContact[i])
                        evtA->onContact[i](a, info);
                }
            }


            if (evtB) {
                info.otherEntity = a;

                for (int i = 0; i < 4; i++) {
                    if (evtB->onContact[i])
                        evtB->onContact[i](b, info);
                }
            }
        }

        void onTrigger(PxTriggerPair* pairs, uint32_t count) override {}

        void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, uint32_t count) override {}
    private:
        entt::registry& reg;
    };

    class ContactModificationCallback : public PxContactModifyCallback {
    public:
        void onContactModify(PxContactModifyPair* const pairs, uint32_t count) override {
            if (callback) callback(callbackCtx, pairs, count);
        }

        void setCallback(void* ctx, ContactModCallback callback) {
            this->callback = callback;
            callbackCtx = ctx;
        }
    private:
        ContactModCallback callback;
        void* callbackCtx;
    };

    SimulationCallback* simCallback;
    ContactModificationCallback* contactModCallback;

    void initPhysx(const EngineInterfaces& interfaces, entt::registry& reg) {
        g_physFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocator, gErrorCallback);
        physScriptEngine = interfaces.scriptEngine;

        physx::PxPvd* pvd = nullptr;
#if ENABLE_PVD
        g_pvd = PxCreatePvd(*g_physFoundation);
        //g_pvdTransport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        g_pvdTransport = physx::PxDefaultPvdFileTransportCreate("blargh");

        pvd = g_pvd;
        bool success = pvd->connect(*g_pvdTransport, physx::PxPvdInstrumentationFlag::eALL);
        if (!success)
            logWarn("Failed to connect to PVD");
#endif

        physx::PxTolerancesScale tolerancesScale;

        g_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_physFoundation, tolerancesScale, true, pvd);

        if (g_physics == nullptr) {
            fatalErr("failed to create physics engine??");
        }

        g_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *g_physFoundation, physx::PxCookingParams(tolerancesScale));
        physx::PxCookingParams params(tolerancesScale);
        params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;

        g_cooking->setParams(params);
        physx::PxSceneDesc desc(tolerancesScale);
        desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        desc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(std::max(SDL_GetCPUCount(), 1));
        desc.filterShader = filterShader;
        desc.solverType = physx::PxSolverType::eTGS;
        desc.flags = PxSceneFlag::eENABLE_CCD
                   | PxSceneFlag::eENABLE_PCM
                   | PxSceneFlag::eENABLE_STABILIZATION;
        g_scene = g_physics->createScene(desc);

        simCallback = new SimulationCallback(reg);
        contactModCallback = new ContactModificationCallback();
        g_scene->setSimulationEventCallback(simCallback);
        g_scene->setContactModifyCallback(contactModCallback);

        reg.on_destroy<PhysicsActor>().connect<&destroyPhysXActor<PhysicsActor>>();
        reg.on_destroy<DynamicPhysicsActor>().connect<&destroyPhysXActor<DynamicPhysicsActor>>();
        reg.on_construct<PhysicsActor>().connect<&setPhysXActorUserdata<PhysicsActor>>();
        reg.on_construct<DynamicPhysicsActor>().connect<&setPhysXActorUserdata<DynamicPhysicsActor>>();

        reg.on_construct<D6Joint>().connect<&setupD6Joint>();
        reg.on_destroy<D6Joint>().connect<&destroyD6Joint>();
        reg.on_construct<FixedJoint>().connect<&setupFixedJoint>();
        reg.on_destroy<FixedJoint>().connect<&destroyFixedJoint>();

        g_console->registerCommand(cmdTogglePhysVis, "phys_toggleVis", "Toggles all physics visualisations.", nullptr);
        g_console->registerCommand(cmdToggleShapeVis, "phys_toggleShapeVis", "Toggles physics shape visualisations.", nullptr);
        g_console->registerCommand(cmdToggleMassAxesVis, "phys_toggleMassAxesVis", "Toggles mass axes visualisations.", nullptr);

        defaultMaterial = g_physics->createMaterial(0.6f, 0.6f, 0.0f);

        dummyBody = g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
        dummyBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
    }

    void stepSimulation(float deltaTime) {
        g_scene->simulate(deltaTime);
        g_scene->fetchResults(true);
    }

    void shutdownPhysx() {
#if ENABLE_PVD
        g_pvd->disconnect();
        g_pvd->release();
        g_pvdTransport->release();
#endif
        g_cooking->release();
        g_physics->release();
        g_physFoundation->release();
    }

    void setContactModCallback(void* ctx, ContactModCallback callback) {
        contactModCallback->setCallback(ctx, callback);
    }

    class RaycastFilterCallback : public PxQueryFilterCallback {
    public:
        uint32_t excludeLayerMask;
        PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override {
            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum preFilter(
                const PxFilterData& filterData, const PxShape* shape,
                const PxRigidActor* actor, PxHitFlags& queryFlags) override {
            const auto& shapeFilterData = shape->getQueryFilterData();

            if ((shapeFilterData.word0 & excludeLayerMask) != 0) {
                return PxQueryHitType::eNONE;
            }

            return PxQueryHitType::eBLOCK;
        }
    };

    bool raycast(physx::PxVec3 position, physx::PxVec3 direction, float maxDist, RaycastHitInfo* hitInfo, uint32_t excludeLayerMask) {
        physx::PxRaycastBuffer hitBuf;
        bool hit;

        if (excludeLayerMask != 0u) {
            RaycastFilterCallback raycastFilterCallback;
            raycastFilterCallback.excludeLayerMask = excludeLayerMask;

            hit = worlds::g_scene->raycast(
                position, direction,
                maxDist, hitBuf,
                PxHitFlag::eDEFAULT,
                PxQueryFilterData(PxFilterData{}, PxQueryFlag::ePREFILTER | PxQueryFlag::eDYNAMIC | PxQueryFlag::eSTATIC),
                &raycastFilterCallback
            );
        } else
            hit = worlds::g_scene->raycast(position, direction, maxDist, hitBuf);

        if (hit && hitInfo) {
            hitInfo->normal = px2glm(hitBuf.block.normal);
            hitInfo->worldPos = px2glm(hitBuf.block.position);
            hitInfo->entity = (entt::entity)(uintptr_t)hitBuf.block.actor->userData;
            hitInfo->distance = direction.dot(hitBuf.block.position - position);
        }

        return hit;
    }

    bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo, uint32_t excludeLayer) {
        return raycast(glm2px(position), glm2px(direction), maxDist, hitInfo, excludeLayer);
    }

    uint32_t overlapSphereMultiple(glm::vec3 origin, float radius, uint32_t maxTouchCount, uint32_t* hitEntityBuffer, uint32_t excludeLayerMask) {
        physx::PxOverlapHit* hitMem = (physx::PxOverlapHit*)alloca(maxTouchCount * sizeof(physx::PxOverlapHit));
        physx::PxSphereGeometry sphereGeo{ radius };
        physx::PxOverlapBuffer hit{ hitMem, maxTouchCount };
        physx::PxQueryFilterData filterData;

        RaycastFilterCallback raycastFilterCallback;
        raycastFilterCallback.excludeLayerMask = excludeLayerMask;

        filterData.flags = physx::PxQueryFlag::eDYNAMIC
            | physx::PxQueryFlag::eSTATIC
            | physx::PxQueryFlag::eNO_BLOCK
            | physx::PxQueryFlag::ePREFILTER;

        physx::PxTransform t{ physx::PxIdentity };
        t.p = glm2px(origin);

        if (!g_scene->overlap(sphereGeo, t, hit, filterData, &raycastFilterCallback)) return 0;

        for (uint32_t i = 0; i < hit.getNbTouches(); i++) {
            const physx::PxOverlapHit& overlap = hit.getTouch(i);
            hitEntityBuffer[i] = (uint32_t)(uintptr_t)overlap.actor->userData;
        }

        return hit.getNbTouches();
    }

    bool sweepSphere(glm::vec3 origin, float radius, glm::vec3 direction, float distance, RaycastHitInfo* hitInfo, uint32_t excludeLayerMask) {
        physx::PxSweepBuffer hitBuf;
        physx::PxSphereGeometry sphere{ radius };
        physx::PxTransform transform{ glm2px(origin) };
        bool hit;

        physx::PxQueryFilterData filterData;
        filterData.flags = physx::PxQueryFlag::eDYNAMIC
            | physx::PxQueryFlag::eSTATIC
            | physx::PxQueryFlag::ePREFILTER;

        if (excludeLayerMask != 0u) {
            RaycastFilterCallback raycastFilterCallback;
            raycastFilterCallback.excludeLayerMask = excludeLayerMask;

            hit = worlds::g_scene->sweep(
                sphere,
                transform,
                glm2px(direction),
                distance,
                hitBuf,
                PxHitFlag::ePOSITION | PxHitFlag::eNORMAL,
                filterData,
                &raycastFilterCallback
            );
        } else
            hit = worlds::g_scene->sweep(sphere, transform, glm2px(direction), distance, hitBuf);

        if (hit && hitInfo) {
            hitInfo->normal = px2glm(hitBuf.block.normal);
            hitInfo->worldPos = px2glm(hitBuf.block.position);
            hitInfo->entity = (entt::entity)(uintptr_t)hitBuf.block.actor->userData;
            hitInfo->distance = hitBuf.block.distance;
        }

        return hit;
    }
}
