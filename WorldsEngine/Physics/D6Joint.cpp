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
        thisActor = other.thisActor; other.thisActor = nullptr;
        targetEntity = other.targetEntity; other.targetEntity = entt::null;
    }

    void D6Joint::operator=(D6Joint&& other) {
        pxJoint = other.pxJoint; other.pxJoint = nullptr;
        thisActor = other.thisActor; other.thisActor = nullptr;
        targetEntity = other.targetEntity; other.targetEntity = entt::null;
    }

    void D6Joint::setTarget(entt::entity newTargetEnt, entt::registry& reg) {
        targetEntity = newTargetEnt;
        auto* pa = reg.try_get<PhysicsActor>(newTargetEnt);
        auto* dpa = reg.try_get<DynamicPhysicsActor>(newTargetEnt);

        if (!(pa || dpa)) {
            logErr("Tried to set a D6 joint's target to an entity with neither a physics actor or dynamic physics actor");
            return;
        }

        auto other = pa ? pa->actor : dpa->actor;

        if (reverseJoint)
            pxJoint->setActors(other, thisActor);
        else
            pxJoint->setActors(thisActor, other);
    }

    physx::PxD6Motion::Enum conv(D6Motion motion) {
        return (physx::PxD6Motion::Enum)motion;
    }

    void D6Joint::setAllLinearMotion(D6Motion wmotion) {
        auto motion = conv(wmotion);
        pxJoint->setMotion(physx::PxD6Axis::eX, motion);
        pxJoint->setMotion(physx::PxD6Axis::eY, motion);
        pxJoint->setMotion(physx::PxD6Axis::eZ, motion);
    }

    void D6Joint::setAllAngularMotion(D6Motion wmotion) {
        auto motion = conv(wmotion);
        pxJoint->setMotion(physx::PxD6Axis::eSWING1, motion);
        pxJoint->setMotion(physx::PxD6Axis::eSWING2, motion);
        pxJoint->setMotion(physx::PxD6Axis::eTWIST, motion);
    }

    entt::entity D6Joint::getTarget() {
        return targetEntity;
    }

    D6Joint::~D6Joint() {
    }
}
