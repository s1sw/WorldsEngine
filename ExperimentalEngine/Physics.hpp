#pragma once
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>

inline physx::PxVec3 glm2Px(glm::vec3 vec) {
	return physx::PxVec3(vec.x, vec.y, vec.z);
}

inline glm::vec3 px2glm(physx::PxVec3 vec) {
	return glm::vec3(vec.x, vec.y, vec.z);
}

extern physx::PxScene* g_scene;
extern physx::PxPhysics* g_physics;
void initPhysx();
void simulate(float deltaTime);
void shutdownPhysx();