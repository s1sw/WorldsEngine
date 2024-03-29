#include "Physics.hpp"
#include "Core/IGameEventHandler.hpp"
#include "D6Joint.hpp"
#include "FixedJoint.hpp"
#include "PhysicsActor.hpp"
#include "Scripting/NetVM.hpp"
#include <Core/Console.hpp>
#include <Core/Fatal.hpp>
#include <ImGui/imgui.h>
#include <SDL_cpuinfo.h>
#include <Util/MathsUtil.hpp>
#include <entt/entity/registry.hpp>
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxSceneDesc.h>
#include <physx/PxSimulationEventCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/pvd/PxPvd.h>
#include <physx/pvd/PxPvdTransport.h>
#include <slib/Intrinsic.hpp>
#include <Tracy.hpp>
using namespace physx;

#define ENABLE_PVD 0

namespace worlds
{
    class PhysErrCallback : public physx::PxErrorCallback
    {
      public:
        virtual void reportError(physx::PxErrorCode::Enum code, const char* msg, const char* file, int line)
        {
            switch (code)
            {
            default:
            case physx::PxErrorCode::eDEBUG_INFO:
                logVrb(WELogCategoryPhysics, "%s (%s:%i)", msg, file, line);
                break;
            case PxErrorCode::eDEBUG_WARNING:
            case PxErrorCode::ePERF_WARNING:
            case PxErrorCode::eINVALID_OPERATION:
            case PxErrorCode::eINVALID_PARAMETER:
                if (strcmp(msg, "PxScene::getRenderBuffer() not allowed while simulation is running.") == 0)
                    return;
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

    bool started = false;

    void* entToPtr(entt::entity ent)
    {
        return (void*)(uintptr_t)(uint32_t)ent;
    }

    entt::entity ptrToEnt(void* ptr)
    {
        return (entt::entity)(uint32_t)(uintptr_t)ptr;
    }

    template <typename T> void destroyPhysXActor(entt::registry& reg, entt::entity ent)
    {
        auto& pa = reg.get<T>(ent);
        pa.actor->release();
    }

    template <typename T> void setPhysXActorUserdata(entt::registry& reg, entt::entity ent)
    {
        auto& pa = reg.get<T>(ent);
        pa.actor->userData = entToPtr(ent);
    }

    void PhysicsSystem::setupD6Joint(entt::registry& reg, entt::entity ent)
    {
        auto& j = reg.get<D6Joint>(ent);
        auto* pxj = physx::PxD6JointCreate(*_physics, dummyBody, physx::PxTransform{physx::PxIdentity}, nullptr,
                                           physx::PxTransform{physx::PxIdentity});

        j.pxJoint = pxj;

        if (!reg.has<RigidBody>(ent))
        {
            logWarn("D6 joint added to entity without a dynamic physics actor");
            return;
        }

        auto& dpa = reg.get<RigidBody>(ent);
        j.thisActor = dpa.actor;
        j.originalThisActor = dpa.actor;
        j.updateJointActors();
    }

    void PhysicsSystem::destroyD6Joint(entt::registry& reg, entt::entity ent)
    {
        auto& j = reg.get<D6Joint>(ent);
        if (j.pxJoint)
        {
            j.pxJoint->release();
        }
    }

    void PhysicsSystem::setupFixedJoint(entt::registry& reg, entt::entity ent)
    {
        auto& j = reg.get<FixedJoint>(ent);

        if (!reg.has<RigidBody>(ent))
        {
            logErr("Fixed joint added to entity without a dynamic physics actor");
            return;
        }

        auto& dpa = reg.get<RigidBody>(ent);
        j.pxJoint = physx::PxFixedJointCreate(*_physics, dpa.actor, physx::PxTransform{physx::PxIdentity}, nullptr,
                                              physx::PxTransform{physx::PxIdentity});
        j.pxJoint->setInvMassScale0(1.0f);
        j.pxJoint->setInvMassScale1(1.0f);
        j.thisActor = dpa.actor;
    }

    void PhysicsSystem::destroyFixedJoint(entt::registry& reg, entt::entity ent)
    {
        auto& j = reg.get<FixedJoint>(ent);
        if (j.pxJoint)
        {
            j.pxJoint->release();
        }
    }

    robin_hood::unordered_map<AssetID, physx::PxTriangleMesh*> physicsTriMesh;

    template <typename T> void PhysicsSystem::updatePhysicsShapes(T& pa, glm::vec3 scale)
    {
        ZoneScoped;
        uint32_t nShapes = pa.actor->getNbShapes();
        physx::PxShape** buf = (physx::PxShape**)std::malloc(nShapes * sizeof(physx::PxShape*));
        pa.actor->getShapes(buf, nShapes);

        for (uint32_t i = 0; i < nShapes; i++)
        {
            pa.actor->detachShape(*buf[i]);
        }

        if (!pa.scaleShapes)
            scale = glm::vec3{1.0f};

        std::free(buf);

        for (PhysicsShape& ps : pa.physicsShapes)
        {
            physx::PxShape* shape;
            physx::PxMaterial* mat = ps.material ? ps.material : _defaultMaterial;

            switch (ps.type)
            {
            case PhysicsShapeType::Box:
                shape = _physics->createShape(physx::PxBoxGeometry(glm2px(ps.box.halfExtents * scale)), *mat);
                break;
            default:
                ps.sphere.radius = 0.5f;
            case PhysicsShapeType::Sphere:
                shape =
                    _physics->createShape(physx::PxSphereGeometry(ps.sphere.radius * glm::compAdd(scale) / 3.0f), *mat);
                break;
            case PhysicsShapeType::Capsule:
                shape =
                    _physics->createShape(physx::PxCapsuleGeometry(ps.capsule.radius, ps.capsule.height * 0.5f), *mat);
                break;
            case PhysicsShapeType::Mesh: {
                if (ps.mesh.mesh == ~0u)
                {
                    logErr(WELogCategoryPhysics, "Mesh collider is missing a mesh!");
                    continue;
                }
                if (!physicsTriMesh.contains(ps.mesh.mesh))
                {
                    const LoadedMesh& lm = MeshManager::loadOrGet(ps.mesh.mesh);

                    std::vector<physx::PxVec3> points;
                    points.resize(lm.vertices.size());

                    for (size_t i = 0; i < lm.vertices.size(); i++)
                    {
                        points[i] = glm2px(lm.vertices[i].position);
                    }

                    physx::PxTriangleMeshDesc meshDesc;

                    meshDesc.points.count = points.size();
                    meshDesc.points.stride = sizeof(physx::PxVec3);
                    meshDesc.points.data = points.data();

                    meshDesc.triangles.count = lm.indices.size() / 3;
                    meshDesc.triangles.data = lm.indices.data();
                    meshDesc.triangles.stride = sizeof(uint32_t) * 3;

                    physx::PxTriangleMesh* triMesh =
                        _cooking->createTriangleMesh(meshDesc, _physics->getPhysicsInsertionCallback());
                    physicsTriMesh.insert({ps.mesh.mesh, triMesh});
                }

                PxMeshScale meshScale{PxVec3{scale.x, scale.y, scale.z}, PxQuat{PxIdentity}};
                shape = _physics->createShape(physx::PxTriangleMeshGeometry(physicsTriMesh.at(ps.mesh.mesh), meshScale),
                                              *mat);
            }
            break;
            case PhysicsShapeType::ConvexMesh: {
                if (ps.convexMesh.mesh == ~0u)
                {
                    logErr(WELogCategoryPhysics, "Convex mesh collider is missing a mesh!");
                    continue;
                }

                const LoadedMesh& lm = MeshManager::loadOrGet(ps.convexMesh.mesh);

                std::vector<physx::PxVec3> points;
                points.resize(lm.vertices.size());

                for (size_t i = 0; i < lm.vertices.size(); i++)
                {
                    points[i] = glm2px(lm.vertices[i].position * scale);
                }

                physx::PxConvexMeshDesc convexDesc;
                convexDesc.points.count = points.size();
                convexDesc.points.stride = sizeof(physx::PxVec3);
                convexDesc.points.data = points.data();
                convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

                physx::PxDefaultMemoryOutputStream buf;
                physx::PxConvexMeshCookingResult::Enum result;

                if (!_cooking->cookConvexMesh(convexDesc, buf, &result))
                {
                    logErr(WELogCategoryPhysics, "Failed to cook mesh %s",
                           AssetDB::idToPath(ps.convexMesh.mesh).c_str());
                    continue;
                }

                physx::PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
                physx::PxConvexMesh* convexMesh = _physics->createConvexMesh(input);

                shape = _physics->createShape(physx::PxConvexMeshGeometry(convexMesh), *mat);
            }
            break;
            }

            shape->setLocalPose(physx::PxTransform{glm2px(ps.pos * scale), glm2px(ps.rot)});
            physx::PxFilterData data;
            data.word0 = pa.layer;
            data.word1 = pa.useContactMod;
            shape->setSimulationFilterData(data);
            shape->setQueryFilterData(data);
            shape->setContactOffset(pa.contactOffset);

            pa.actor->attachShape(*shape);
            shape->release();
        }
    }

    template void PhysicsSystem::updatePhysicsShapes<PhysicsActor>(PhysicsActor& pa, glm::vec3 scale);
    template void PhysicsSystem::updatePhysicsShapes<RigidBody>(RigidBody& pa, glm::vec3 scale);

    uint32_t physicsLayerMask[32] = {0xFFFFFFFF, 0xFFFFFFFD, 0x00000000, 0xFFFFFFFF};

    static physx::PxFilterFlags filterShader(physx::PxFilterObjectAttributes attributes1, physx::PxFilterData data1,
                                             physx::PxFilterObjectAttributes attributes2, physx::PxFilterData data2,
                                             physx::PxPairFlags& pairFlags, const void*, physx::PxU32)
    {

        int layer1 = slib::Intrinsics::bitScanForward(data1.word0);
        int layer2 = slib::Intrinsics::bitScanForward(data2.word0);

        if ((physicsLayerMask[layer1] & data2.word0) == 0 || (physicsLayerMask[layer2] & data1.word0) == 0)
            return physx::PxFilterFlag::eKILL;

        pairFlags = physx::PxPairFlag::eSOLVE_CONTACT | physx::PxPairFlag::eDETECT_DISCRETE_CONTACT |
                    physx::PxPairFlag::eDETECT_CCD_CONTACT | physx::PxPairFlag::eNOTIFY_TOUCH_FOUND |
                    physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;

        if (data1.word1 || data2.word1)
        {
            pairFlags |= PxPairFlag::eMODIFY_CONTACTS;
        }

        return physx::PxFilterFlags();
    }

    DotNetScriptEngine* physScriptEngine;

    class SimulationCallback : public PxSimulationEventCallback
    {
      public:
        SimulationCallback(entt::registry& reg) : reg{reg}
        {
        }

        void onConstraintBreak(PxConstraintInfo* constraints, uint32_t count) override
        {
        }

        void onWake(PxActor** actors, uint32_t count) override
        {
        }

        void onSleep(PxActor** actors, uint32_t count) override
        {
        }

        void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, uint32_t nbPairs) override
        {
            entt::entity a = ptrToEnt(pairHeader.actors[0]->userData);
            entt::entity b = ptrToEnt(pairHeader.actors[1]->userData);

            auto evtA = reg.try_get<PhysicsEvents>(a);
            auto evtB = reg.try_get<PhysicsEvents>(b);

            glm::vec3 velA{0.0f};
            glm::vec3 velB{0.0f};

            auto aDynamic = pairHeader.actors[0]->is<PxRigidDynamic>();
            auto bDynamic = pairHeader.actors[1]->is<PxRigidDynamic>();

            if (aDynamic)
            {
                velA = px2glm(aDynamic->getLinearVelocity());
            }

            if (bDynamic)
            {
                velB = px2glm(bDynamic->getLinearVelocity());
            }

            PhysicsContactInfo info{.relativeSpeed = glm::distance(velA, velB)};

            const uint32_t contactBufSize = 32;
            PxContactPairPoint contacts[contactBufSize];
            uint32_t totalContacts = 0;

            for (uint32_t i = 0; i < nbPairs; i++)
            {
                auto& pair = pairs[i];
                PxU32 nbContacts = pair.extractContacts(contacts, contactBufSize);

                for (uint32_t j = 0; j < nbContacts; j++)
                {
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

            if (evtA == nullptr && evtB == nullptr)
                return;

            if (evtA)
            {
                info.otherEntity = b;

                for (int i = 0; i < 4; i++)
                {
                    if (evtA->onContact[i])
                        evtA->onContact[i](a, info);
                }
            }

            if (evtB)
            {
                info.otherEntity = a;

                for (int i = 0; i < 4; i++)
                {
                    if (evtB->onContact[i])
                        evtB->onContact[i](b, info);
                }
            }
        }

        void onTrigger(PxTriggerPair* pairs, uint32_t count) override
        {
        }

        void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, uint32_t count) override
        {
        }

      private:
        entt::registry& reg;
    };

    class ContactModificationCallback : public PxContactModifyCallback
    {
      public:
        void onContactModify(PxContactModifyPair* const pairs, uint32_t count) override
        {
            if (callback)
                callback(callbackCtx, pairs, count);
        }

        void setCallback(void* ctx, ContactModCallback callback)
        {
            this->callback = callback;
            callbackCtx = ctx;
        }

      private:
        ContactModCallback callback;
        void* callbackCtx;
    };

    SimulationCallback* simCallback;
    ContactModificationCallback* contactModCallback;

    PhysicsSystem::PhysicsSystem(const EngineInterfaces& interfaces, entt::registry& reg) : reg(reg)
    {
        errorCallback = new PhysErrCallback;
        foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, *errorCallback);
        physScriptEngine = interfaces.scriptEngine;
        logVrb("PhysX version: %i.%i.%i", PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);

        physx::PxPvd* pvd = nullptr;
#if ENABLE_PVD
        g_pvd = PxCreatePvd(*g_physFoundation);
        // g_pvdTransport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        g_pvdTransport = physx::PxDefaultPvdFileTransportCreate("blargh");

        pvd = g_pvd;
        bool success = pvd->connect(*g_pvdTransport, physx::PxPvdInstrumentationFlag::eALL);
        if (!success)
            logWarn("Failed to connect to PVD");
#endif

        physx::PxTolerancesScale tolerancesScale;

        _physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, tolerancesScale, true, pvd);

        if (_physics == nullptr)
        {
            fatalErr("failed to create physics engine??");
        }

        _cooking = PxCreateCooking(PX_PHYSICS_VERSION, *foundation, physx::PxCookingParams(tolerancesScale));
        physx::PxCookingParams params(tolerancesScale);
        params.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;
        params.meshWeldTolerance = 0.00001f;

        _cooking->setParams(params);
        physx::PxSceneDesc desc(tolerancesScale);
        desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        int numThreads = std::min(SDL_GetCPUCount(), 8);
        desc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(numThreads);
        logMsg(WELogCategoryPhysics, "Using %u threads.", desc.cpuDispatcher->getWorkerCount());
        desc.filterShader = filterShader;
        desc.solverType = physx::PxSolverType::eTGS;
        desc.flags = PxSceneFlag::eENABLE_CCD | PxSceneFlag::eENABLE_PCM;
        desc.bounceThresholdVelocity = 2.0f;
        _scene = _physics->createScene(desc);

        simCallback = new SimulationCallback(reg);
        contactModCallback = new ContactModificationCallback();
        _scene->setSimulationEventCallback(simCallback);
        _scene->setContactModifyCallback(contactModCallback);

        reg.on_destroy<PhysicsActor>().connect<&destroyPhysXActor<PhysicsActor>>();
        reg.on_destroy<RigidBody>().connect<&destroyPhysXActor<RigidBody>>();
        reg.on_construct<PhysicsActor>().connect<&setPhysXActorUserdata<PhysicsActor>>();
        reg.on_construct<RigidBody>().connect<&setPhysXActorUserdata<RigidBody>>();

        reg.on_construct<D6Joint>().connect<&PhysicsSystem::setupD6Joint>(this);
        reg.on_destroy<D6Joint>().connect<&PhysicsSystem::destroyD6Joint>(this);
        reg.on_construct<FixedJoint>().connect<&PhysicsSystem::setupFixedJoint>(this);
        reg.on_destroy<FixedJoint>().connect<&PhysicsSystem::destroyFixedJoint>(this);

        g_console->registerCommand(
            [&](const char*) {
                float currentScale = _scene->getVisualizationParameter(physx::PxVisualizationParameter::eSCALE);

                _scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f - currentScale);
            },
            "phys_toggleVis", "Toggles all physics visualisations.");
        g_console->registerCommand(
            [&](const char*) {
                float currentVal =
                    _scene->getVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES);

                _scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES,
                                                  1.0f - currentVal);
            },
            "phys_toggleShapeVis", "Toggles physics shape visualisations.");

        _defaultMaterial = _physics->createMaterial(0.6f, 0.6f, 0.0f);

        dummyBody = _physics->createRigidDynamic(physx::PxTransform{physx::PxIdentity});
        dummyBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
    }

    void PhysicsSystem::stepSimulation(float deltaTime)
    {
        _scene->simulate(deltaTime);
        _scene->fetchResults(true);
    }

    void PhysicsSystem::resetMeshCache()
    {
        for (auto& kv : physicsTriMesh)
        {
            kv.second->release();
        }
        physicsTriMesh.clear();

        reg.view<PhysicsActor, Transform>().each(
            [this](PhysicsActor& pa, Transform& t) { updatePhysicsShapes(pa, t.scale); });

        reg.view<RigidBody, Transform>().each(
            [this](RigidBody& pa, Transform& t) { updatePhysicsShapes(pa, t.scale); });
    }

    PhysicsSystem::~PhysicsSystem()
    {
#if ENABLE_PVD
        g_pvd->disconnect();
        g_pvd->release();
        g_pvdTransport->release();
#endif
        _cooking->release();
        _physics->release();
        foundation->release();
    }

    void PhysicsSystem::setContactModCallback(void* ctx, ContactModCallback callback)
    {
        contactModCallback->setCallback(ctx, callback);
    }

    class RaycastFilterCallback : public PxQueryFilterCallback
    {
      public:
        uint32_t excludeLayerMask;
        PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
        {
            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor,
                                       PxHitFlags& queryFlags) override
        {
            const auto& shapeFilterData = shape->getQueryFilterData();

            if ((shapeFilterData.word0 & excludeLayerMask) != 0)
            {
                return PxQueryHitType::eNONE;
            }

            return PxQueryHitType::eBLOCK;
        }
    };

    bool PhysicsSystem::raycast(glm::vec3 position, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo,
                                uint32_t excludeLayerMask)
    {
        physx::PxRaycastBuffer hitBuf;
        bool hit;

        if (excludeLayerMask != 0u)
        {
            RaycastFilterCallback raycastFilterCallback;
            raycastFilterCallback.excludeLayerMask = excludeLayerMask;

            hit = _scene->raycast(glm2px(position), glm2px(direction), maxDist, hitBuf, PxHitFlag::eDEFAULT,
                                  PxQueryFilterData(PxFilterData{}, PxQueryFlag::ePREFILTER | PxQueryFlag::eDYNAMIC |
                                                                        PxQueryFlag::eSTATIC),
                                  &raycastFilterCallback);
        }
        else
            hit = _scene->raycast(glm2px(position), glm2px(direction), maxDist, hitBuf);

        if (hit && hitInfo)
        {
            hitInfo->normal = px2glm(hitBuf.block.normal);
            hitInfo->worldPos = px2glm(hitBuf.block.position);
            hitInfo->entity = (entt::entity)(uintptr_t)hitBuf.block.actor->userData;
            hitInfo->distance = glm::dot(direction, px2glm(hitBuf.block.position) - position);
            hitInfo->hitLayer = slib::Intrinsics::bitScanForward(hitBuf.block.shape->getQueryFilterData().word0);
        }

        return hit;
    }

    uint32_t PhysicsSystem::overlapSphereMultiple(glm::vec3 origin, float radius, uint32_t maxTouchCount,
                                                  entt::entity* hitEntityBuffer, uint32_t excludeLayerMask)
    {
        physx::PxOverlapHit* hitMem = (physx::PxOverlapHit*)alloca(maxTouchCount * sizeof(physx::PxOverlapHit));
        physx::PxSphereGeometry sphereGeo{radius};
        physx::PxOverlapBuffer hit{hitMem, maxTouchCount};
        physx::PxQueryFilterData filterData;

        RaycastFilterCallback raycastFilterCallback;
        raycastFilterCallback.excludeLayerMask = excludeLayerMask;

        filterData.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eNO_BLOCK |
                           physx::PxQueryFlag::ePREFILTER;

        physx::PxTransform t{physx::PxIdentity};
        t.p = glm2px(origin);

        if (!_scene->overlap(sphereGeo, t, hit, filterData, &raycastFilterCallback))
            return 0;

        for (uint32_t i = 0; i < hit.getNbTouches(); i++)
        {
            const physx::PxOverlapHit& overlap = hit.getTouch(i);
            hitEntityBuffer[i] = ptrToEnt(overlap.actor->userData);
        }

        return hit.getNbTouches();
    }

    bool PhysicsSystem::sweepSphere(glm::vec3 origin, float radius, glm::vec3 direction, float distance,
                                    RaycastHitInfo* hitInfo, uint32_t excludeLayerMask)
    {
        physx::PxSweepBuffer hitBuf;
        physx::PxSphereGeometry sphere{radius};
        physx::PxTransform transform{glm2px(origin)};
        bool hit;

        physx::PxQueryFilterData filterData;
        filterData.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

        if (excludeLayerMask != 0u)
        {
            RaycastFilterCallback raycastFilterCallback;
            raycastFilterCallback.excludeLayerMask = excludeLayerMask;

            hit = _scene->sweep(sphere, transform, glm2px(direction), distance, hitBuf,
                                PxHitFlag::ePOSITION | PxHitFlag::eNORMAL, filterData, &raycastFilterCallback);
        }
        else
            hit = _scene->sweep(sphere, transform, glm2px(direction), distance, hitBuf);

        if (hit && hitInfo)
        {
            hitInfo->normal = px2glm(hitBuf.block.normal);
            hitInfo->worldPos = px2glm(hitBuf.block.position);
            hitInfo->entity = ptrToEnt(hitBuf.block.actor->userData);
            hitInfo->distance = hitBuf.block.distance;
        }

        return hit;
    }
}
