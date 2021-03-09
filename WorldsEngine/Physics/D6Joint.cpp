#include "D6Joint.hpp"
#include <physx/PxPhysicsAPI.h>
#include <physx/extensions/PxJoint.h>
#include <physx/extensions/PxD6Joint.h>
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "../Core/Log.hpp"

namespace worlds {
    D6Joint::D6Joint() : pxJoint{ nullptr }, thisActor{ nullptr }, targetEntity{ entt::null } {}

    D6Joint::D6Joint(D6Joint&& other) noexcept { 
        pxJoint = other.pxJoint; other.pxJoint = nullptr; 
    }

    void D6Joint::operator=(D6Joint&& other) { 
        pxJoint = other.pxJoint; 
        other.pxJoint = nullptr; 
    }

    void D6Joint::setTarget(entt::entity newTargetEnt, entt::registry& reg) {
        targetEntity = newTargetEnt;
        auto* pa = reg.try_get<PhysicsActor>(newTargetEnt);
        auto* dpa = reg.try_get<DynamicPhysicsActor>(newTargetEnt);

        if (!(pa || dpa)) {
            logErr("Tried to set a D6 joint's target to an entity with neither a physics actor or dynamic physics actor");
            return;
        }

        if (pa) {
            pxJoint->setActors(thisActor, pa->actor);
        } else {
            pxJoint->setActors(thisActor, dpa->actor);
        }
    }

    entt::entity D6Joint::getTarget() {
        return targetEntity;
    }

    D6Joint::~D6Joint() {
    }
}
