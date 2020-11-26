#include "PhysHandController.hpp"
#include "Physics.hpp"
#include "PxRigidBody.h"
#include "MathsUtil.hpp"

namespace converge {

    PhysHandController::PhysHandController(
        entt::entity handEnt, 
        const PIDSettings& posSettings,
        const PIDSettings& rotSettings,
        entt::registry& registry
    ) 
        : handEnt { handEnt } 
        , registry { registry } {

        posPid.acceptSettings(posSettings);
        rotPid.acceptSettings(rotSettings);

        body = static_cast<physx::PxRigidBody*>(registry.get<worlds::DynamicPhysicsActor>(handEnt).actor);
    }

    void PhysHandController::setTargetTransform(const Transform& t) {
        targetPos = t.position;
        targetRot = t.rotation;
    }

    void PhysHandController::applyForces(float simStep) {
        physx::PxTransform t = body->getGlobalPose();
        
        glm::vec3 err = targetPos - worlds::px2glm(t.p);
        glm::vec3 force = posPid.getOutput(err, simStep);

        body->addForce(worlds::glm2px(force));
        forceMag[currDbgIdx] = glm::length(force);

        glm::quat filteredQ = glm::normalize(targetRot);
        filteredQ = fixupQuat(filteredQ);

        glm::quat quatDiff = filteredQ * glm::inverse(fixupQuat(worlds::px2glm(t.q)));
        quatDiff = fixupQuat(quatDiff);

        float angle = glm::angle(quatDiff);
        glm::vec3 axis = glm::axis(quatDiff);
        angle = glm::degrees(angle);
        angle = AngleToErr(angle);
        angle = glm::radians(angle);

        glm::vec3 torque = rotPid.getOutput(angle * axis, simStep);

        torqueMag[currDbgIdx] = glm::length(torque);
        if (!glm::any(glm::isnan(axis)) && !glm::any(glm::isinf(torque)))
            body->addTorque(worlds::glm2px(torque));
    }
}
