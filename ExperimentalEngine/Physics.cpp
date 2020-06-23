#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>

physx::PxDefaultErrorCallback gDefaultErrorCallback;
physx::PxDefaultAllocator gDefaultAllocator;
physx::PxFoundation* g_physFoundation;
physx::PxPhysics* g_physics;
physx::PxPvd* g_pvd;
physx::PxCooking* g_cooking;
physx::PxScene* g_scene;
void initPhysx() {
	g_physFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocator, gDefaultErrorCallback);

	g_pvd = PxCreatePvd(*g_physFoundation);
	physx::PxPvdTransport* pvdTransport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);

	physx::PxTolerancesScale tolerancesScale;

	g_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_physFoundation, tolerancesScale, true, g_pvd);

	if (g_physics == nullptr) {
		__debugbreak();
	}

	g_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *g_physFoundation, physx::PxCookingParams(tolerancesScale));
	physx::PxSceneDesc desc(tolerancesScale);
	desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	desc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(8);
	desc.filterShader = physx::PxDefaultSimulationFilterShader;
	g_scene = g_physics->createScene(desc);
	g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
	g_scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
}

void simulate(float deltaTime) {
	g_scene->simulate(deltaTime);
	g_scene->fetchResults(true);
}

void shutdownPhysx() {
	g_cooking->release();
	g_physics->release();
	g_physFoundation->release();
}