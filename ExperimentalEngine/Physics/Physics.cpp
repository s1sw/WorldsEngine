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
                case physx::PxErrorCode::eDEBUG_WARNING:
                case physx::PxErrorCode::ePERF_WARNING:
                case physx::PxErrorCode::eINVALID_OPERATION:
                case physx::PxErrorCode::eINVALID_PARAMETER:
                    logWarn(WELogCategoryPhysics, "%s (%s:%i)", msg, file, line);
                    break;
                case physx::PxErrorCode::eINTERNAL_ERROR:
                case physx::PxErrorCode::eABORT:
                case physx::PxErrorCode::eOUT_OF_MEMORY:
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
        physx::PxFilterData,
        physx::PxFilterObjectAttributes,
        physx::PxFilterData,
        physx::PxPairFlags& pairFlags,
        const void*,
        physx::PxU32) {
        pairFlags = physx::PxPairFlag::eSOLVE_CONTACT;
        pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
        pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
        return physx::PxFilterFlags();
    }


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
        desc.solverType = physx::PxSolverType::ePGS;
        desc.ccdMaxPasses = 5;
        desc.bounceThresholdVelocity = 5.0f;
        desc.flags = physx::PxSceneFlag::eENABLE_PCM 
                   | physx::PxSceneFlag::eENABLE_CCD;
        desc.frictionType = physx::PxFrictionType::ePATCH;
        g_scene = g_physics->createScene(desc);

        reg.on_destroy<PhysicsActor>().connect<&destroyPhysXActor<PhysicsActor>>();
        reg.on_destroy<DynamicPhysicsActor>().connect<&destroyPhysXActor<DynamicPhysicsActor>>();
        reg.on_construct<PhysicsActor>().connect<&setPhysXActorUserdata<PhysicsActor>>();
        reg.on_construct<DynamicPhysicsActor>().connect<&setPhysXActorUserdata<DynamicPhysicsActor>>();

        reg.on_construct<D6Joint>().connect<&setupD6Joint>();

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

    bool raycast(physx::PxVec3 position, physx::PxVec3 direction, float maxDist, RaycastHitInfo* hitInfo) {
        physx::PxRaycastBuffer hitBuf;
        bool hit = worlds::g_scene->raycast(position, direction, maxDist, hitBuf);
        hit &= hitBuf.hasBlock;

        if (hit && hitInfo) {
            hitInfo->normal = px2glm(hitBuf.block.normal);
            hitInfo->worldPos = px2glm(hitBuf.block.position);
            hitInfo->entity = (entt::entity)(uintptr_t)hitBuf.block.actor->userData;
        }

        return hit;
    }

    bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo) {
        return raycast(glm2px(position), glm2px(direction), maxDist, hitInfo);
    }
}
