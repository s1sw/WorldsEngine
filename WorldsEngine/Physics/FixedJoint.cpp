#include "FixedJoint.hpp"
#include <physx/PxPhysicsAPI.h>
#include <physx/extensions/PxJoint.h>
#include <physx/extensions/PxFixedJoint.h>
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "../Core/Log.hpp"

namespace worlds {
    FixedJoint::FixedJoint() : pxJoint{ nullptr }, thisActor { nullptr }, targetEntity { entt::null } {}

    FixedJoint::FixedJoint(FixedJoint&& other) noexcept {
        pxJoint = other.pxJoint;
        other.pxJoint = nullptr;
    }

    void FixedJoint::operator=(FixedJoint&& other) {
        pxJoint = other.pxJoint;
        other.pxJoint = nullptr;
    }

    void FixedJoint::setTarget(entt::entity newTargetEnt, entt::registry& reg) {
        targetEntity = newTargetEnt;
        auto* pa = reg.try_get<PhysicsActor>(newTargetEnt);
        auto* dpa = reg.try_get<RigidBody>(newTargetEnt);

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

    entt::entity FixedJoint::getTarget() {
        return targetEntity;
    }

    FixedJoint::~FixedJoint() {
    }
}
