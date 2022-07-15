#include "PhysicsActor.hpp"
#include <Physics/Physics.hpp>
#include <physx/PxRigidDynamic.h>

namespace worlds
{
    glm::vec3 RigidBody::linearVelocity() const
    {
        return px2glm(actor->getLinearVelocity());
    }

    glm::vec3 RigidBody::angularVelocity() const
    {
        return px2glm(actor->getAngularVelocity());
    }

    void RigidBody::setLinearVelocity(glm::vec3 vel)
    {
        actor->setLinearVelocity(glm2px(vel));
    }

    void RigidBody::setAngularVelocity(glm::vec3 vel)
    {
        actor->setAngularVelocity(glm2px(vel));
    }

    void RigidBody::addForce(glm::vec3 force, ForceMode forceMode)
    {
        physx::PxForceMode::Enum pxForceMode = (physx::PxForceMode::Enum)forceMode;
        actor->addForce(glm2px(force), pxForceMode);
    }

    void RigidBody::addTorque(glm::vec3 torque, ForceMode forceMode)
    {
        physx::PxForceMode::Enum pxForceMode = (physx::PxForceMode::Enum)forceMode;
        actor->addTorque(glm2px(torque), pxForceMode);
    }

    void RigidBody::addForceAtPosition(glm::vec3 force, glm::vec3 position, ForceMode forceMode)
    {
        physx::PxForceMode::Enum pxForceMode = (physx::PxForceMode::Enum)forceMode;

        physx::PxRigidBodyExt::addForceAtPos(*actor, glm2px(force), glm2px(position), pxForceMode);
    }

    Transform RigidBody::pose() const
    {
        return px2glm(actor->getGlobalPose());
    }

    void RigidBody::setPose(const Transform& t)
    {
        actor->setGlobalPose(glm2px(t));
    }

    DPALockFlags RigidBody::lockFlags() const
    {
        return (DPALockFlags)(uint32_t)actor->getRigidDynamicLockFlags();
    }

    void RigidBody::setLockFlags(DPALockFlags flags)
    {
        actor->setRigidDynamicLockFlags((physx::PxRigidDynamicLockFlag::Enum)flags);
    }

    float RigidBody::maxAngularVelocity() const
    {
        return actor->getMaxAngularVelocity();
    }

    void RigidBody::setMaxAngularVelocity(float vel)
    {
        actor->setMaxAngularVelocity(vel);
    }

    float RigidBody::maxLinearVelocity() const
    {
        return actor->getMaxLinearVelocity();
    }

    void RigidBody::setMaxLinearVelocity(float vel)
    {
        actor->setMaxLinearVelocity(vel);
    }

    void RigidBody::setEnabled(bool enabled)
    {
        actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, !enabled);
    }

    bool RigidBody::enabled() const
    {
        return (actor->getActorFlags() & physx::PxActorFlag::eDISABLE_SIMULATION) == (physx::PxActorFlag::Enum)0;
    }
}
