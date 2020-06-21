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
	g_scene = g_physics->createScene(physx::PxSceneDesc(tolerancesScale));
}

void simulate(float deltaTime) {
	g_scene->simulate(deltaTime);
	g_scene->fetchResults(true);
	uint32_t numActiveActors = 0;
	physx::PxActor** activeActors = g_scene->getActiveActors(numActiveActors);

	for (int i = 0; i < numActiveActors; i++) {
		
	}
}

void shutdownPhysx() {
	g_cooking->release();
	g_physics->release();
	g_physFoundation->release();
}