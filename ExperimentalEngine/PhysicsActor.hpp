#pragma once
#include <physx/PxRigidActor.h>

struct PhysicsActor {
	PhysicsActor(physx::PxRigidActor* actor) : actor(actor) {}
	physx::PxRigidActor* actor;
};

struct DynamicPhysicsActor {
	DynamicPhysicsActor(physx::PxRigidActor* actor) : actor(actor) {}
	physx::PxRigidActor* actor;
};