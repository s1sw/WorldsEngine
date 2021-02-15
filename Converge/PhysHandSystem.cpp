#include "PhysHandSystem.hpp"
#include <Core/Log.hpp>
#include <Render/Camera.hpp>
#include <Physics/Physics.hpp>
#include <physx/PxRigidBody.h>
#include "MathsUtil.hpp"
#include <openvr.h>

namespace converge {
    PhysHandSystem::PhysHandSystem(worlds::EngineInterfaces interfaces, entt::registry& registry) 
        : interfaces {interfaces} 
        , registry {registry} {
    }

    void PhysHandSystem::update(entt::registry& registry, float deltaTime, float) {
        registry.view<PhysHand>().each([&](entt::entity ent, PhysHand& physHand) {
            setTargets(physHand, ent, deltaTime);
        });
    }

    void PhysHandSystem::simulate(entt::registry& registry, float simStep) {
        registry.view<PhysHand, worlds::DynamicPhysicsActor>().each([&](auto, PhysHand& physHand, worlds::DynamicPhysicsActor& actor) {
            auto body = static_cast<physx::PxRigidBody*>(actor.actor);
            physx::PxTransform t = body->getGlobalPose();

            if (!body->getLinearVelocity().isFinite()) {
                logErr("physhand velocity was not finite, resetting...");
                body->setLinearVelocity(physx::PxVec3{ 0.0f });
            }
            
            glm::vec3 err = physHand.targetWorldPos - worlds::px2glm(t.p);


            glm::vec3 vel = worlds::px2glm(body->getLinearVelocity());
            glm::vec3 refVel{ 0.0f };
            
            if (registry.valid(physHand.locosphere)) {
                worlds::DynamicPhysicsActor& lDpa = registry.get<worlds::DynamicPhysicsActor>(physHand.locosphere);
                refVel = worlds::px2glm(((physx::PxRigidDynamic*)lDpa.actor)->getLinearVelocity());
            }

            glm::vec3 force = physHand.posController.getOutput(worlds::px2glm(t.p) - refVel * simStep * 2.0f, physHand.targetWorldPos, vel, simStep, refVel);

            if (!glm::any(glm::isnan(force)))
                body->addForce(worlds::glm2px(force));

            if (registry.valid(physHand.locosphere)) {
                worlds::DynamicPhysicsActor& lDpa = registry.get<worlds::DynamicPhysicsActor>(physHand.locosphere);
                lDpa.actor->is<physx::PxRigidBody>()->addForce(-worlds::glm2px(force));
            }

            // sometimes the rotations we get are really awful :(
            // fix that and deal with it
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

    void PhysHandSystem::setTargets(PhysHand& hand, entt::entity ent, float deltaTime) {
        if (hand.follow == FollowHand::None) return;

        worlds::Hand wHand;
        switch (hand.follow) {
        default:
        case FollowHand::LeftHand:
            wHand = worlds::Hand::LeftHand;
            break;
        case FollowHand::RightHand:
            wHand = worlds::Hand::RightHand;
            break;
        }

        glm::vec3 posOffset { 0.0f, 0.01f, 0.1f };
        glm::vec3 rotEulerOffset { -60.0f, 0.0f, 0.0f };


        Transform t;
        if (interfaces.vrInterface->getHandTransform(wHand, t)) {
            t.position += t.rotation * posOffset;
            t.fromMatrix(glm::inverse(interfaces.mainCamera->getViewMatrix()) * t.getMatrix());
            t.rotation *= glm::quat{glm::radians(rotEulerOffset)};

            hand.targetWorldPos = t.position;
            hand.targetWorldRot = t.rotation;
        }
    }
}
