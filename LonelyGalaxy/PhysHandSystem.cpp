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
#include <Input/Input.hpp>
#include "LocospherePlayerSystem.hpp"

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
    worlds::ConVar killHands { "lg_killHands", "0", "Bleh" };
    worlds::ConVar handTuning { "lg_handTuning", "0", "Displays a debug menu for tuning hand PID controllers." };
    worlds::ConVar useHandLimits { "lg_useHandLimits", "1", "Use force+torque limits on hands." };

    void PhysHandSystem::preSimUpdate(entt::registry& registry, float deltaTime) {
        registry.view<PhysHand>().each([&](entt::entity ent, PhysHand& physHand) {
            //setTargets(physHand, ent, deltaTime);
        });

        if (handTuning.getInt()) {
            ImGui::DragFloat3("Euler Hand Offset", glm::value_ptr(rotEulerOffset));
        }
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


    void PhysHandSystem::simulate(entt::registry& registry, float simStep) {
        registry.view<PhysHand, worlds::DynamicPhysicsActor>().each([&](entt::entity ent, PhysHand& physHand, worlds::DynamicPhysicsActor& actor) {
            setTargets(physHand, ent, simStep);
            auto body = static_cast<physx::PxRigidBody*>(actor.actor);
            physx::PxTransform t = body->getGlobalPose();

            if (!t.p.isFinite() || !body->getLinearVelocity().isFinite() || !body->getLinearVelocity().isFinite()) {
                logErr("physhand rb was not finite, resetting...");
                resetHand(physHand, body);
                t = body->getGlobalPose();
            }

            glm::vec3 refVel{ 0.0f };

            if (registry.valid(physHand.locosphere)) {
                worlds::DynamicPhysicsActor& lDpa = registry.get<worlds::DynamicPhysicsActor>(physHand.locosphere);
                refVel = lDpa.linearVelocity();

                if (physHand.follow != FollowHand::None) {
                    // Avoid applying force if it's just going to be limited by the arm joint
                    const float maxDist = 0.8f;
                    glm::vec3 headPos = interfaces.mainCamera->position;

                    if (interfaces.vrInterface) {
                        glm::vec3 hmdPos = interfaces.mainCamera->rotation * worlds::getMatrixTranslation(interfaces.vrInterface->getHeadTransform());
                        hmdPos.x = -hmdPos.x;
                        hmdPos.z = -hmdPos.z;
                        headPos += hmdPos;
                    }

                    glm::vec3 dir = physHand.targetWorldPos - headPos;
                    dir = clampMagnitude(dir, maxDist);
                    physHand.targetWorldPos = headPos + dir;
                }
            }

            actor.addForce(refVel - physHand.lastRefVel, worlds::ForceMode::VelocityChange);

            glm::vec3 err = (physHand.targetWorldPos) - worlds::px2glm(t.p);
            physHand.lastRefVel = refVel;
            glm::vec3 vel = worlds::px2glm(body->getLinearVelocity());

            glm::vec3 force = physHand.posController.getOutput(err * physHand.forceMultiplier, simStep);
            if (useHandLimits.getInt())
                force = clampMagnitude(force, physHand.forceLimit);

            if (glm::any(glm::isnan(force))) {
                force = glm::vec3(0.0f);
            }

            if (!killHands.getInt())
                body->addForce(worlds::glm2px(force));

            if (!killHands.getInt() && registry.valid(physHand.locosphere)) {
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

            torque = glm::inverse(fixupQuat(worlds::px2glm(t.q))) * torque;

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

            torque = fixupQuat(worlds::px2glm(t.q)) * torque;

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

            if (useHandLimits.getInt())
                torque = clampMagnitude(torque, physHand.torqueLimit);

            if (!killHands.getInt() && !glm::any(glm::isnan(axis)) && !glm::any(glm::isinf(torque)))
                body->addTorque(worlds::glm2px(torque));
        });
    }

    extern glm::vec3 nextCamPos;

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
                LocospherePlayerComponent& lpc = registry.get<LocospherePlayerComponent>(hand.locosphere);
                glm::quat virtualRotation = interfaces.mainCamera->rotation * glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));

                t.position += t.rotation * posOffset;
                t.position = virtualRotation * t.position;

                t.position += lpc.headPos;

                t.rotation *= glm::quat{glm::radians(rotEulerOffset)};

                hand.targetWorldPos = t.position;
                hand.targetWorldRot = virtualRotation * t.rotation;
            }
        } else {
            static glm::vec3 camOffset { 0.1f, -0.1f, 0.55f };

            worlds::DynamicPhysicsActor& locosphereDpa = registry.get<worlds::DynamicPhysicsActor>(hand.locosphere);

            if (interfaces.inputManager->keyPressed(SDL_SCANCODE_KP_8))
                camOffset.z += 0.05f;

            if (interfaces.inputManager->keyPressed(SDL_SCANCODE_KP_2))
                camOffset.z -= 0.05f;

            if (hand.follow == FollowHand::RightHand)
                camOffset.x = -camOffset.x;

            hand.targetWorldPos = worlds::px2glm(locosphereDpa.actor->getGlobalPose().p);
            hand.targetWorldPos += glm::vec3{0.0f, 1.55f, 0.0f};
            hand.targetWorldPos += interfaces.mainCamera->rotation * camOffset;
            hand.targetWorldRot = interfaces.mainCamera->rotation;
            if (hand.follow == FollowHand::RightHand)
                camOffset.x = -camOffset.x;
        }
    }
}
