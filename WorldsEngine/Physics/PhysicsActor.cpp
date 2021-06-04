#include "PhysicsActor.hpp"
#include "Physics/Physics.hpp"
#include "PxRigidDynamic.h"

namespace worlds {
    glm::vec3 DynamicPhysicsActor::linearVelocity() const {
        return px2glm(actor->getLinearVelocity());
    }

    glm::vec3 DynamicPhysicsActor::angularVelocity() const {
        return px2glm(actor->getAngularVelocity());
    }

    void DynamicPhysicsActor::setLinearVelocity(glm::vec3 vel) {
        actor->setLinearVelocity(glm2px(vel));
    }

    void DynamicPhysicsActor::setAngularVelocity(glm::vec3 vel) {
        actor->setAngularVelocity(glm2px(vel));
    }

    void DynamicPhysicsActor::addForce(glm::vec3 force, ForceMode forceMode) {
        physx::PxForceMode::Enum pxForceMode = (physx::PxForceMode::Enum)forceMode;
        actor->addForce(glm2px(force), pxForceMode);
    }

    Transform DynamicPhysicsActor::pose() const {
        return px2glm(actor->getGlobalPose());
    }

    void DynamicPhysicsActor::setPose(const Transform& t) {
        actor->setGlobalPose(glm2px(t));
    }
}
