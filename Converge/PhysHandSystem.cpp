#include "PhysHandSystem.hpp"
#include "Physics.hpp"
#include <physx/PxRigidBody.h>
#include "MathsUtil.hpp"

namespace converge {
    PhysHandSystem::PhysHandSystem(worlds::EngineInterfaces, entt::registry& registry) 
        : registry{registry} {
    }

    void PhysHandSystem::simulate(entt::registry& registry, float simStep) {
        registry.view<PhysHand, worlds::DynamicPhysicsActor>().each([&](auto, PhysHand& physHand, worlds::DynamicPhysicsActor& actor) {
            auto body = static_cast<physx::PxRigidBody*>(actor.actor);
            physx::PxTransform t = body->getGlobalPose();
            
            glm::vec3 err = physHand.targetWorldPos - worlds::px2glm(t.p);
            glm::vec3 force = physHand.posController.getOutput(err, simStep);

            body->addForce(worlds::glm2px(force));

            glm::quat filteredQ = glm::normalize(physHand.targetWorldRot);
            filteredQ = fixupQuat(filteredQ);

            glm::quat quatDiff = filteredQ * glm::inverse(fixupQuat(worlds::px2glm(t.q)));
            quatDiff = fixupQuat(quatDiff);

            float angle = glm::angle(quatDiff);
            glm::vec3 axis = glm::axis(quatDiff);
            angle = glm::degrees(angle);
            angle = AngleToErr(angle);
            angle = glm::radians(angle);

            glm::vec3 torque = physHand.rotController.getOutput(angle * axis, simStep);

            if (!glm::any(glm::isnan(axis)) && !glm::any(glm::isinf(torque)))
                body->addTorque(worlds::glm2px(torque));
        });
    }
}
