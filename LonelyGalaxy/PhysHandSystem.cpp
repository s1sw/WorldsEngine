#include "PhysHandSystem.hpp"
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <Render/Camera.hpp>
#include <Physics/Physics.hpp>
#include <physx/PxRigidBody.h>
#include "MathsUtil.hpp"
#include "Physics/D6Joint.hpp"
#include "PxQueryFiltering.h"
#include "extensions/PxJoint.h"
#include <openvr.h>
#include <ImGui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <Core/Console.hpp>
#include "DebugArrow.hpp"
#include <VR/OpenVRInterface.hpp>

namespace lg {
    void resetHand(PhysHand& ph, physx::PxRigidBody* rb) {
        physx::PxTransform t;
        rb->setLinearVelocity(physx::PxVec3(0.0f));
        rb->setAngularVelocity(physx::PxVec3(0.0f));
        t.p = worlds::glm2px(ph.targetWorldPos);
        t.q = worlds::glm2px(ph.targetWorldRot);
        rb->setGlobalPose(t);
    }

    PhysHandSystem::PhysHandSystem(worlds::EngineInterfaces interfaces, entt::registry& registry)
        : interfaces {interfaces}
        , registry {registry} {
        worlds::g_console->registerCommand([&](void*, const char*) {
            registry.view<PhysHand, worlds::DynamicPhysicsActor>().each([&](auto, PhysHand& ph, worlds::DynamicPhysicsActor& dpa) {
                resetHand(ph, static_cast<physx::PxRigidBody*>(dpa.actor));
            });
        }, "lg_resetHands", "Resets hands.", nullptr);
    }

    static glm::vec3 posOffset { 0.0f, 0.0f, -0.05f };
    static glm::vec3 rotEulerOffset { -120.0f, 0.0f, -51.0f };
    worlds::ConVar physHandDbg { "lg_physHandDbg", "0", "Show debug menu for physics hands" };

    void PhysHandSystem::preSimUpdate(entt::registry& registry, float deltaTime) {
        registry.view<PhysHand>().each([&](entt::entity ent, PhysHand& physHand) {
            setTargets(physHand, ent, deltaTime);
        });
    }

    glm::vec3 getAxisOfLargestComponent(glm::vec3 v3) {
        v3 = glm::abs(v3);
        if (v3.x > v3.y && v3.x > v3.z) {
            return glm::vec3{1.0f, 0.0f, 0.0f};
        } else if (v3.y > v3.x && v3.y > v3.z) {
            return glm::vec3{0.0f, 1.0f, 0.0f};
        } else if (v3.z > v3.x && v3.z > v3.y) {
            return glm::vec3{0.0f, 0.0f, 1.0f};
        }
        return glm::vec3{0.0f, 0.0f, 1.0f};
    }

    worlds::ConVar handTuning { "lg_handTuning", "0", "Displays a debug menu for tuning hand PID controllers." };

    void PhysHandSystem::simulate(entt::registry& registry, float simStep) {
        registry.view<PhysHand, worlds::DynamicPhysicsActor, Transform>().each([&](entt::entity ent, PhysHand& physHand, worlds::DynamicPhysicsActor& actor, Transform& wtf) {
            //setTargets(physHand, ent, simStep);
            auto body = static_cast<physx::PxRigidBody*>(actor.actor);
            physx::PxTransform t = body->getGlobalPose();

            if (!t.p.isFinite() || !t.q.isSane() || !body->getLinearVelocity().isFinite() || !body->getLinearVelocity().isFinite()) {
                logErr("physhand rb was not finite, resetting...");
                resetHand(physHand, body);
                t = body->getGlobalPose();
            }

            glm::vec3 refVel{ 0.0f };

            if (registry.valid(physHand.locosphere)) {
                worlds::DynamicPhysicsActor& lDpa = registry.get<worlds::DynamicPhysicsActor>(physHand.locosphere);
                refVel = worlds::px2glm(((physx::PxRigidDynamic*)lDpa.actor)->getLinearVelocity());
            }

            glm::vec3 err = (physHand.targetWorldPos + refVel * simStep) - worlds::px2glm(t.p) + ((body->getMass() / physHand.posController.P) * glm::vec3(0.0f, 9.81f, 0.0f));
            glm::vec3 vel = worlds::px2glm(body->getLinearVelocity());

            glm::vec3 force = physHand.posController.getOutput(err * physHand.forceMultiplier, simStep);
            force = clampMagnitude(force, physHand.forceLimit);

            if (glm::any(glm::isnan(force))) {
                force = glm::vec3(0.0f);
            }

            body->addForce(worlds::glm2px(force));

            if (registry.valid(physHand.locosphere)) {
                worlds::DynamicPhysicsActor& lDpa = registry.get<worlds::DynamicPhysicsActor>(physHand.locosphere);
                lDpa.actor->is<physx::PxRigidBody>()->addForce(-worlds::glm2px(force));
            }

            glm::quat quatDiff = fixupQuat(physHand.targetWorldRot) * glm::inverse(fixupQuat(worlds::px2glm(t.q)));
            quatDiff = fixupQuat(quatDiff);

            float angle = glm::angle(quatDiff);
            glm::vec3 axis = glm::axis(quatDiff);
            angle = glm::degrees(angle);
            angle = AngleToErr(angle);
            angle = glm::radians(angle);

            glm::vec3 torque = physHand.rotController.getOutput(angle * axis, simStep);

            torque = glm::inverse(fixupQuat(wtf.rotation)) * torque;

            if (!physHand.useOverrideIT) {
                auto itRotation = worlds::px2glm(body->getCMassLocalPose().q);
                auto inertiaTensor = worlds::px2glm(body->getMassSpaceInertiaTensor());

                // transform torque to mass space
                torque = itRotation * torque;

                torque *= inertiaTensor;

                // and back to world space
                torque = glm::inverse(itRotation) * torque;
            } else {
                torque = worlds::px2glm(physHand.overrideIT.transform(worlds::glm2px(torque)));
            }

            torque = fixupQuat(wtf.rotation) * torque;

            auto& nc = registry.get<worlds::NameComponent>(ent);
            if (handTuning.getInt()) {
                ImGui::PushID(nc.name.c_str());
                ImGui::Text("%s", nc.name.c_str());
                ImGui::DragFloat3("Rpid", &physHand.rotController.P, 0.1f);
                ImGui::DragFloat3("Ppid", &physHand.posController.P);
                ImGui::DragFloat("pos I avg amt", &physHand.posController.averageAmount);
                ImGui::DragFloat("rot I avg amt", &physHand.rotController.averageAmount);
                auto linVel = body->getLinearVelocity();
                auto angVel = body->getAngularVelocity();
                ImGui::Text("Lin Vel: %.3f, %.3f, %.3f", linVel.x, linVel.y, linVel.z);
                ImGui::Text("Ang Vel: %.3f, %.3f, %.3f", angVel.x, angVel.y, angVel.z);
                ImGui::PopID();
            }

            torque = clampMagnitude(torque, physHand.torqueLimit);

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

        if (interfaces.vrInterface) {
            Transform t;
            if (interfaces.vrInterface->getHandTransform(wHand, t)) {
                t.position += t.rotation * posOffset;
                glm::quat flip180 = glm::angleAxis(glm::pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f});
                glm::quat correctedRot = interfaces.mainCamera->rotation * flip180;
                t.position = correctedRot * t.position;
                t.position += interfaces.mainCamera->position;
                t.rotation *= glm::quat{glm::radians(rotEulerOffset)};
                t.rotation = flip180 * interfaces.mainCamera->rotation * t.rotation;

                hand.targetWorldPos = t.position;
                hand.targetWorldRot = t.rotation;
            }
        } else {
            glm::vec3 camOffset { 0.1f, -0.1f, 0.25f };
            if (hand.follow == FollowHand::RightHand)
                camOffset.x = -camOffset.x;
            hand.targetWorldPos = interfaces.mainCamera->position;
            hand.targetWorldPos += interfaces.mainCamera->rotation * camOffset;
            hand.targetWorldRot = interfaces.mainCamera->rotation;
        }
    }
}
