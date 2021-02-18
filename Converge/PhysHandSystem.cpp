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

namespace converge {
	struct FilterEntity : public physx::PxQueryFilterCallback {
        uint32_t entA;
        uint32_t entB;

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override {
            if ((uint32_t)(uintptr_t)actor->userData == entA || (uint32_t)(uintptr_t)actor->userData == entB) {
                return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }


        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit) override {
            if ((uint32_t)(uintptr_t)hit.actor->userData == entA || (uint32_t)(uintptr_t)hit.actor->userData == entB) {
                return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
	};

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
        registry.view<PhysHand, worlds::DynamicPhysicsActor>().each([&](entt::entity ent, PhysHand& physHand, worlds::DynamicPhysicsActor& actor) {
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

            if (physHand.gripPressed) {
                logMsg("grip pressed");
                // search for nearby grabbable objects
                physx::PxSphereGeometry sphereGeo{0.15f};
                physx::PxOverlapBuffer hit;
                physx::PxQueryFilterData filterData;
                filterData.flags = physx::PxQueryFlag::eDYNAMIC
                                 | physx::PxQueryFlag::eSTATIC
                                 | physx::PxQueryFlag::eANY_HIT 
                                 | physx::PxQueryFlag::ePOSTFILTER;
                
                FilterEntity filterEnt;
                filterEnt.entA = (uint32_t)ent;
                filterEnt.entB = (uint32_t)(entt::entity)entt::null;
                
                if (worlds::g_scene->overlap(sphereGeo, t, hit, filterData, &filterEnt)) {
                    logMsg("overlap %i touches %i hits", hit.nbTouches, hit.getNbAnyHits());
                    const auto& touch = hit.getAnyHit(0);
                    if (touch.actor == nullptr)
                        logErr("touch actor is nullptr");
                    // take the 0th hit for now
                    auto pickUp = (entt::entity)(uint32_t)(uintptr_t)touch.actor->userData;

                    if (registry.has<worlds::NameComponent>(pickUp)) {
                        logMsg("trying to grab %s", registry.get<worlds::NameComponent>(pickUp).name.c_str());
                    }

                    auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                    physx::PxTransform p = body->getGlobalPose();
                    physx::PxTransform p2 = touch.actor->getGlobalPose();
                    d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, p.transformInv(p2));
                    d6.setTarget(pickUp, registry);
                }
            }

            if (physHand.gripReleased) {
                if (registry.has<worlds::D6Joint>(ent)) {
                    registry.remove<worlds::D6Joint>(ent);
                    logMsg("removed d6");
                }
            }
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
