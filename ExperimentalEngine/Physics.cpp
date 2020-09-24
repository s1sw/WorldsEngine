#include "PCH.hpp"
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>
#include "PhysicsActor.hpp"
#include "Console.hpp"
#include "imgui.h"
#include <SDL2/SDL_cpuinfo.h>

#define ENABLE_PVD 0

namespace worlds {
    physx::PxDefaultErrorCallback gDefaultErrorCallback;
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

    template <typename T>
    void destroyPhysXActor(entt::registry& reg, entt::entity ent) {
        auto& pa = reg.get<T>(ent);
        pa.actor->release();
    }

    void cmdTogglePhysVis(void* obj, const char* arg) {
        float currentScale = g_scene->getVisualizationParameter(physx::PxVisualizationParameter::eSCALE);
        
        g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f - currentScale);
    }

    void cmdToggleShapeVis(void* obj, const char* arg) {
        float currentVal = g_scene->getVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES);

        g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f - currentVal);
    }

    static physx::PxFilterFlags filterShader(
        physx::PxFilterObjectAttributes attributes0,
        physx::PxFilterData filterData0,
        physx::PxFilterObjectAttributes attributes1,
        physx::PxFilterData filterData1,
        physx::PxPairFlags& pairFlags,
        const void* constantBlock,
        physx::PxU32 constantBlockSize) {
        pairFlags = physx::PxPairFlag::eSOLVE_CONTACT;
        pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
        pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
        return physx::PxFilterFlags();
    }


    void initPhysx(entt::registry& reg) {
        g_physFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocator, gDefaultErrorCallback);

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
            __debugbreak();
        }

        g_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *g_physFoundation, physx::PxCookingParams(tolerancesScale));
        physx::PxSceneDesc desc(tolerancesScale);
        desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        desc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(std::max(SDL_GetCPUCount() - 2, 1));
        desc.filterShader = filterShader;
        desc.solverType = physx::PxSolverType::ePGS;
        desc.ccdMaxPasses = 5;
        desc.bounceThresholdVelocity = 2.0f;
        desc.flags = physx::PxSceneFlag::eENABLE_PCM 
                   | physx::PxSceneFlag::eENABLE_CCD;
        g_scene = g_physics->createScene(desc);
        reg.on_destroy<PhysicsActor>().connect<&destroyPhysXActor<PhysicsActor>>();
        reg.on_destroy<DynamicPhysicsActor>().connect<&destroyPhysXActor<DynamicPhysicsActor>>();
        g_console->registerCommand(cmdTogglePhysVis, "phys_toggleVis", "Toggles all physics visualisations.", nullptr);
        g_console->registerCommand(cmdToggleShapeVis, "phys_toggleShapeVis", "Toggles physics shape visualisations.", nullptr);
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
}