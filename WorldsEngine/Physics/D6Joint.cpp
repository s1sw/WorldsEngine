#include "D6Joint.hpp"
#include <physx/PxPhysicsAPI.h>
#include <physx/extensions/PxJoint.h>
#include <physx/extensions/PxD6Joint.h>
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "../Core/Log.hpp"
#include <Physics/Physics.hpp>

namespace worlds {
    D6Joint::D6Joint() : pxJoint{ nullptr }, thisActor{ nullptr }, targetEntity{ entt::null } {}

    D6Joint::D6Joint(D6Joint&& other) noexcept {
        pxJoint = other.pxJoint; other.pxJoint = nullptr;
        thisActor = other.thisActor; other.thisActor = nullptr;
        targetEntity = other.targetEntity; other.targetEntity = entt::null;
        replaceThis = other.replaceThis; other.replaceThis = entt::null;
        originalThisActor = other.originalThisActor; other.originalThisActor = nullptr;
    }

    void D6Joint::operator=(D6Joint&& other) {
        pxJoint = other.pxJoint; other.pxJoint = nullptr;
        thisActor = other.thisActor; other.thisActor = nullptr;
        targetEntity = other.targetEntity; other.targetEntity = entt::null;
        replaceThis = other.replaceThis; other.replaceThis = entt::null;
        originalThisActor = other.originalThisActor; other.originalThisActor = nullptr;
    }

    physx::PxRigidActor* getSuitablePhysicsActor(entt::entity ent, entt::registry& reg) {
        auto* pa = reg.try_get<PhysicsActor>(ent);
        auto* dpa = reg.try_get<RigidBody>(ent);

        if (!(pa || dpa)) {
            return nullptr;
        }

        return pa ? pa->actor : dpa->actor;
    }

    void D6Joint::setTarget(entt::entity newTargetEnt, entt::registry& reg) {
        targetEntity = newTargetEnt;
        auto* pa = reg.try_get<PhysicsActor>(newTargetEnt);
        auto* dpa = reg.try_get<RigidBody>(newTargetEnt);

        if (!(pa || dpa)) {
            logErr("Tried to set a D6 joint's target to an entity with neither a physics actor or dynamic physics actor");
            return;
        }

        targetActor = getSuitablePhysicsActor(newTargetEnt, reg);
        updateJointActors();
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

    entt::entity D6Joint::getAttached() {
        return replaceThis;
    }

    void D6Joint::setAttached(entt::entity ent, entt::registry& reg) {
        replaceThis = ent;

        if (ent != entt::null) {
            thisActor = getSuitablePhysicsActor(ent, reg);
        } else {
            thisActor = originalThisActor;
        }

        updateJointActors();
    }

    D6Joint::~D6Joint() {
    }

    void D6Joint::updateJointActors() {
        if (reverseJoint)
            pxJoint->setActors(targetActor, thisActor);
        else
            pxJoint->setActors(thisActor, targetActor);
    }
}
