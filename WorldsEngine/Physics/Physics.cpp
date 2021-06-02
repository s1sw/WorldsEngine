#include "PCH.hpp"
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>
#include "PhysicsActor.hpp"
#include "../Core/Console.hpp"
#include "../ImGui/imgui.h"
#include <SDL_cpuinfo.h>
#include "../Core/Fatal.hpp"
#include "Physics.hpp"
#include "D6Joint.hpp"
#include "FixedJoint.hpp"
#include "PxSceneDesc.h"
#include "PxSimulationEventCallback.h"
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
        if (!reg.has<DynamicPhysicsActor>(ent)) {
            logErr("D6 joint added to entity without a dynamic physics actor");
            return;
        }

        auto& dpa = reg.get<DynamicPhysicsActor>(ent);
        j.pxJoint = physx::PxD6JointCreate(*g_physics, dpa.actor, physx::PxTransform{ physx::PxIdentity }, nullptr, physx::PxTransform{ physx::PxIdentity });
        j.thisActor = dpa.actor;
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

    static physx::PxFilterFlags filterShader(
        physx::PxFilterObjectAttributes,
        physx::PxFilterData data1,
        physx::PxFilterObjectAttributes,
        physx::PxFilterData data2,
        physx::PxPairFlags& pairFlags,
        const void*,
        physx::PxU32) {
        if (data1.word0 == PLAYER_PHYSICS_LAYER && data2.word0 == PLAYER_PHYSICS_LAYER)
            return physx::PxFilterFlag::eKILL;
        if (data1.word0 == NOCOLLISION_PHYSICS_LAYER || data2.word0 == NOCOLLISION_PHYSICS_LAYER)
            return physx::PxFilterFlag::eKILL;

        pairFlags = physx::PxPairFlag::eSOLVE_CONTACT
                  | physx::PxPairFlag::eDETECT_DISCRETE_CONTACT
                  | physx::PxPairFlag::eDETECT_CCD_CONTACT
                  | physx::PxPairFlag::eNOTIFY_TOUCH_FOUND
                  | physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
        return physx::PxFilterFlags();
    }

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

            if (evtA == nullptr && evtB == nullptr) return;

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
                }
            }

            info.averageContactPoint /= totalContacts;

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

    SimulationCallback* simCallback;

    void initPhysx(entt::registry& reg) {
        g_physFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocator, gErrorCallback);

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
        physx::PxSceneDesc desc(tolerancesScale);
        desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        desc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(std::max(SDL_GetCPUCount() - 2, 1));
        desc.filterShader = filterShader;
        desc.solverType = physx::PxSolverType::eTGS;
        desc.flags |= PxSceneFlag::eENABLE_CCD;
        g_scene = g_physics->createScene(desc);

        simCallback = new SimulationCallback(reg);
        g_scene->setSimulationEventCallback(simCallback);

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

        defaultMaterial = g_physics->createMaterial(0.6f, 0.6f, 0.0f);
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

    class RaycastFilterCallback : public PxQueryFilterCallback {
    public:
        uint32_t excludeLayer;
        PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override {
            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum preFilter(
                const PxFilterData& filterData, const PxShape* shape,
                const PxRigidActor* actor, PxHitFlags& queryFlags) override {
            const auto& shapeFilterData = shape->getQueryFilterData();

            if (shapeFilterData.word0 == excludeLayer) {
                return PxQueryHitType::eNONE;
            }

            return PxQueryHitType::eBLOCK;
        }
    };
    RaycastFilterCallback raycastFilterCallback;

    bool raycast(physx::PxVec3 position, physx::PxVec3 direction, float maxDist, RaycastHitInfo* hitInfo, uint32_t excludeLayer) {
        physx::PxRaycastBuffer hitBuf;
        bool hit;

        if (excludeLayer != ~0u) {
            raycastFilterCallback.excludeLayer = excludeLayer;

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
            hitInfo->distance = hitBuf.block.distance;
        }

        return hit;
    }

    bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo, uint32_t excludeLayer) {
        return raycast(glm2px(position), glm2px(direction), maxDist, hitInfo, excludeLayer);
    }
}
