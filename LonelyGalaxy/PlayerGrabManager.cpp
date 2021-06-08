#include "PlayerGrabManager.hpp"
#include <Input/Input.hpp>
#include <Physics/D6Joint.hpp>
#include "Core/Console.hpp"
#include "Core/Log.hpp"
#include "GripPoint.hpp"
#include "ImGui/imgui.h"
#include "MathsUtil.hpp"
#include "Physics/FilterEntities.hpp"
#include "Scripting/ScriptComponent.hpp"
#include "Scripting/WrenVM.hpp"
#include "physxit.h"
#include <Util/CreateModelObject.hpp>
#include "Grabbable.hpp"

namespace lg {
    worlds::ConVar itCompDbg { "lg_itCompDbg", "0", "Shows physics shapes for grabbed objects." };
    worlds::ConVar useTensorCompensation{"lg_compensateTensors", "1", "Enables inertia tensor compensation on grabs."};
    worlds::ConVar enableGripPoints { "lg_enableGripPoints", "1", "Enables grip points." };

    void setAllAxisD6Motion(physx::PxD6Joint* j, physx::PxD6Motion::Enum motion) {
        j->setMotion(physx::PxD6Axis::eX, motion);
        j->setMotion(physx::PxD6Axis::eY, motion);
        j->setMotion(physx::PxD6Axis::eZ, motion);
        j->setMotion(physx::PxD6Axis::eSWING1, motion);
        j->setMotion(physx::PxD6Axis::eSWING2, motion);
        j->setMotion(physx::PxD6Axis::eTWIST, motion);
    }

    void addShapeTensor(entt::registry& reg, worlds::PhysicsShape& shape, physx::IT::InertiaTensorComputer& itComp,
            physx::PxTransform shapeTransform, physx::PxTransform handTransform,
            glm::vec3 scale,
            physx::PxTransform shapeWSTransform = physx::PxTransform{physx::PxIdentity}, bool showWS = false) {
        physx::IT::InertiaTensorComputer shapeComp(false);

        shapeTransform.p.multiply(worlds::glm2px(scale));

        physx::PxTransform wsTransform = handTransform * shapeTransform;

        if (itCompDbg.getInt()) {
            if (showWS) {
                worlds::createModelObject(reg, worlds::px2glm(shapeWSTransform.p), worlds::px2glm(shapeWSTransform.q),
                        worlds::AssetDB::pathToId(shape.type == worlds::PhysicsShapeType::Sphere ?
                            "uvsphere.obj" : "model.obj"),
                        worlds::AssetDB::pathToId("Materials/dev.json"),
                        shape.type == worlds::PhysicsShapeType::Sphere ?
                        glm::vec3{shape.sphere.radius * 0.5f} :
                        shape.box.halfExtents * scale);
            } else {
                worlds::createModelObject(reg, worlds::px2glm(wsTransform.p), worlds::px2glm(wsTransform.q),
                        worlds::AssetDB::pathToId(shape.type == worlds::PhysicsShapeType::Sphere ?
                            "uvsphere.obj" : "model.obj"),
                        worlds::AssetDB::pathToId("Materials/dev.json"),
                        shape.type == worlds::PhysicsShapeType::Sphere ?
                        glm::vec3{shape.sphere.radius * 0.5f} :
                        shape.box.halfExtents * scale);
            }
        }

        switch (shape.type) {
        case worlds::PhysicsShapeType::Sphere:
            shapeComp.setSphere(shape.sphere.radius * glm::compAdd(scale) / 3.0f, &shapeTransform);
            break;
        case worlds::PhysicsShapeType::Box:
            shapeComp.setBox(worlds::glm2px(shape.box.halfExtents * scale), &shapeTransform);
            break;
        case worlds::PhysicsShapeType::Capsule:
            shapeComp.setCapsule(0, shape.capsule.radius, shape.capsule.height, &shapeTransform);
            break;
        default:
            logErr("unknown shape type used in inertia tensor calculation");
            break;
        }

        itComp.add(shapeComp);
    }

    void setPhysHandTensor(PhysHand& hand,
            worlds::DynamicPhysicsActor& handDpa, worlds::DynamicPhysicsActor& dpa,
            const physx::PxTransform& handT, const Transform& objectT,
            entt::registry& reg) {
        // find offset of other physics actor
        auto otherT = dpa.actor->getGlobalPose();

        // calculate combined inertia tensor
        physx::IT::InertiaTensorComputer itComp(true);

        for (auto& shape : dpa.physicsShapes) {
            auto worldSpace = otherT * physx::PxTransform(worlds::glm2px(shape.pos), worlds::glm2px(shape.rot));
            auto handSpace = handT.getInverse() * worldSpace;
            auto scale = dpa.scaleShapes ? objectT.scale : glm::vec3{1.0f};

            addShapeTensor(reg, shape, itComp, handSpace, handT, scale, worldSpace, false);
        }

        for (auto& shape : handDpa.physicsShapes) {
            auto shapeT = physx::PxTransform(worlds::glm2px(shape.pos), worlds::glm2px(shape.rot));
            addShapeTensor(reg, shape, itComp, shapeT, handT, glm::vec3{1.0f});
        }

        itComp.scaleDensity((handDpa.mass + dpa.mass) / itComp.getMass());

        physx::PxMat33 it = itComp.getInertia();

        hand.overrideIT = it;
        hand.rotController.reset();
    }

    PlayerGrabManager::PlayerGrabManager(
            worlds::EngineInterfaces interfaces,
            entt::registry& registry)
        : playerEnt{entt::null}
        , registry{registry}
        , interfaces{interfaces} {
        if (interfaces.vrInterface) {
            auto vrInterface = interfaces.vrInterface;
            lGrab = vrInterface->getActionHandle("/actions/main/in/GrabL");
            rGrab = vrInterface->getActionHandle("/actions/main/in/GrabR");
            lTrigger = vrInterface->getActionHandle("/actions/main/in/TriggerL");
            rTrigger = vrInterface->getActionHandle("/actions/main/in/TriggerR");
        }
    }

    void PlayerGrabManager::simulate(float simStep) {
        if (!registry.valid(playerEnt)) return;
        auto& localRig = registry.get<PlayerRig>(playerEnt);

        updateHandGrab(localRig, localRig.lHand, simStep);
        updateHandGrab(localRig, localRig.rHand, simStep);
    }

    void PlayerGrabManager::setPlayerEntity(entt::entity playerEnt) {
        this->playerEnt = playerEnt;
    }

    void PlayerGrabManager::updateHandGrab(PlayerRig& rig, entt::entity ent, float deltaTime) {
        auto vrInterface = interfaces.vrInterface;
        auto inputManager = interfaces.inputManager;
        auto& physHand = registry.get<PhysHand>(ent);
        auto grabAction = physHand.follow == FollowHand::LeftHand ? lGrab : rGrab;
        auto grabButton = physHand.follow == FollowHand::LeftHand ? worlds::MouseButton::Left : worlds::MouseButton::Right;
        bool doGrab = vrInterface ? vrInterface->getActionPressed(grabAction) : inputManager->mouseButtonPressed(grabButton);
        bool doRelease = vrInterface ? vrInterface->getActionReleased(grabAction) : inputManager->mouseButtonReleased(grabButton);
        auto& handTf = registry.get<Transform>(ent);
        auto& dpa = registry.get<worlds::DynamicPhysicsActor>(ent);

        if (registry.valid(physHand.currentlyGrabbed)) {
            Grabbable& grabbable = registry.get<Grabbable>(physHand.currentlyGrabbed);
            auto triggerAction = physHand.follow == FollowHand::LeftHand ? lTrigger : rTrigger;

            if (vrInterface ) {
                if (vrInterface->getActionPressed(triggerAction)) {
                    if (grabbable.onTriggerPressed)
                        grabbable.onTriggerPressed(physHand.currentlyGrabbed);
                }

                if (vrInterface->getActionHeld(triggerAction)) {
                    if (grabbable.onTriggerHeld)
                        grabbable.onTriggerHeld(physHand.currentlyGrabbed);
                }

                if (vrInterface->getActionReleased(triggerAction)) {
                    if (grabbable.onTriggerReleased)
                        grabbable.onTriggerReleased(physHand.currentlyGrabbed);
                }
            }
        }

        if (registry.valid(physHand.goingTo)) {
            auto& otherActor = registry.get<worlds::DynamicPhysicsActor>(physHand.goingTo);
            if (doRelease) {
                physHand.goingTo = entt::null;
                physHand.follow = physHand.oldFollowHand;
                registry.remove_if_exists<worlds::D6Joint>(ent);
                otherActor.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
                return;
            }

            auto otherTf = worlds::px2glm(otherActor.actor->getGlobalPose());
            Grabbable& grabbable = registry.get<Grabbable>(physHand.goingTo);
            Grip& grip = grabbable.grips[physHand.gripIndex];

            Transform gripTransform = grip.getWorldSpace(otherTf);
            float distance = glm::distance(handTf.position, gripTransform.position);

            if (distance > 0.5f) {
                logMsg("too far");
                physHand.goingTo = entt::null;
                physHand.follow = physHand.oldFollowHand;
                registry.remove_if_exists<worlds::D6Joint>(ent);
                otherActor.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
                return;
            }

            float rotDot = grip.calcRotationAlignment(handTf.rotation, otherTf);
            ImGui::Text("%.3f distance, %.3f rotDot", distance, rotDot);
            glm::vec3 displacement = gripTransform.position - handTf.position;
            ImGui::Text("displacement: %.3f, %.3f, %.3f", displacement.x, displacement.y, displacement.z);

            physHand.targetWorldPos = gripTransform.position;
            physHand.targetWorldRot = gripTransform.rotation;

            if (distance < 0.03f && rotDot > 0.85f) {
                auto& d6 = registry.get<worlds::D6Joint>(ent);
                d6.setTarget(physHand.goingTo, registry);

                logMsg("hit target, locking joint for grab");
                physHand.useOverrideIT = true;
                physHand.currentlyGrabbed = physHand.goingTo;
                physHand.goingTo = entt::null;

                physx::PxTransform target {
                    worlds::glm2px(grip.position),
                    worlds::glm2px(grip.rotation)
                };

                auto objectT = otherActor.actor->getGlobalPose();
                d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, target);
                d6.pxJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
                setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
                if (useTensorCompensation.getInt()) {
                    setPhysHandTensor(physHand, dpa, otherActor, worlds::glm2px(Transform{gripTransform.position, gripTransform.rotation}), worlds::px2glm(objectT), registry);
                }

                physHand.useOverrideIT = true;
                physHand.follow = physHand.oldFollowHand;
                physHand.forceMultiplier = 1.0f;

                otherActor.layer = worlds::PLAYER_PHYSICS_LAYER;
                worlds::updatePhysicsShapes(otherActor);
            }
        }

        if (doGrab && !registry.valid(physHand.goingTo) && !registry.has<worlds::D6Joint>(ent)) {
            // search for nearby grabbable objects
            physx::PxSphereGeometry sphereGeo{0.1f};
            physx::PxOverlapBuffer hit;
            physx::PxQueryFilterData filterData;
            filterData.flags = physx::PxQueryFlag::eDYNAMIC
                             | physx::PxQueryFlag::eSTATIC
                             | physx::PxQueryFlag::eANY_HIT
                             | physx::PxQueryFlag::ePOSTFILTER;

            worlds::FilterComponent<Grabbable> filter{registry};
            auto t = dpa.actor->getGlobalPose();
            auto overlapCenter = t;
            overlapCenter.p += t.q.rotate(worlds::glm2px(dpa.physicsShapes[0].pos));

            if (worlds::g_scene->overlap(sphereGeo, overlapCenter, hit, filterData, &filter)) {
                const auto& touch = hit.getAnyHit(0);
                auto pickUp = (entt::entity)(uint32_t)(uintptr_t)touch.actor->userData;

                if (registry.valid(pickUp) && registry.valid(ent)) {
                    handleGrab(pickUp, ent);
                }
            }
        }

        if (doRelease && registry.has<worlds::D6Joint>(ent)) {
            auto& ph = registry.get<PhysHand>(ent);
            auto heldEnt = ph.currentlyGrabbed;
            worlds::DynamicPhysicsActor* heldDpa = registry.try_get<worlds::DynamicPhysicsActor>(ent);

            if (heldDpa) {
                heldDpa->layer = worlds::DEFAULT_PHYSICS_LAYER;
                worlds::updatePhysicsShapes(*heldDpa);
            }

            if (registry.valid(heldEnt)) {
                Grabbable& grabbable = registry.get<Grabbable>(heldEnt);
                grabbable.grips[ph.gripIndex].inUse = false;
            }

            registry.remove<worlds::D6Joint>(ent);
            ph.useOverrideIT = false;
            ph.forceMultiplier = 1.0f;
            ph.currentlyGrabbed = entt::null;
        }
    }

    float PlayerGrabManager::calculateGripScore(Grip& grip, const Transform& handTransform, const Transform& grabbingTransform) {
        float linearScore = 1.0f / grip.calcDistance(handTransform.position, grabbingTransform);
        float angularScore =  grip.calcRotationAlignment(handTransform.rotation, grabbingTransform);

        return linearScore;
    }

    void PlayerGrabManager::handleGrab(entt::entity grabbing, entt::entity hand) {
        PhysHand& physHand = registry.get<PhysHand>(hand);


        const Transform& handTransform = registry.get<Transform>(hand);
        const Transform& grabbingTransform = registry.get<Transform>(grabbing);
        auto& grabbable = registry.get<Grabbable>(grabbing);

        logMsg("grabbing object");

        if (grabbable.grips.size() > 0 && enableGripPoints.getInt()) {
            GripHand gripHand = physHand.follow == FollowHand::LeftHand ? GripHand::Left : GripHand::Right;
            // Select which grip we're going to use
            // First, copy to a temporary vector
            std::vector<int> potentialGripIndices;

            for (size_t i = 0; i < grabbable.grips.size(); i++) {
                potentialGripIndices.push_back(i);
            }

            // Now filter out grips which don't apply
            potentialGripIndices.erase(std::remove_if(potentialGripIndices.begin(), potentialGripIndices.end(),
                [&](int gripIndex) {
                    Grip& grip = grabbable.grips[gripIndex];
                    return (grip.inUse && grip.exclusive) ||
                           (grip.hand != GripHand::Both && grip.hand != gripHand);
                }), potentialGripIndices.end()
            );

            // No available grips, exit now
            if (potentialGripIndices.size() == 0) return;

            if (registry.has<worlds::ScriptComponent>(grabbing)) {
                interfaces.scriptEngine->fireEvent(grabbing, "onGrab");
            }

            int i = 0;
            for (Grip& g : grabbable.grips) {
                logMsg("Grip %i: %.3f", i, calculateGripScore(g, handTransform, grabbingTransform));
                i++;
            }

            // Sort by "appropriateness score"
            // (1 / distance) * dot(handRotation, gripRotation)
            std::sort(potentialGripIndices.begin(), potentialGripIndices.end(),
                [&](int gripIdxA, int gripIdxB) {
                    Grip& gripA = grabbable.grips[gripIdxA];
                    Grip& gripB = grabbable.grips[gripIdxB];

                    float aScore = calculateGripScore(gripA, handTransform, grabbingTransform);
                    float bScore = calculateGripScore(gripB, handTransform, grabbingTransform);

                    return aScore > bScore;
                }
            );

            int bestGripIndex = potentialGripIndices[0];
            Grip& bestGrip = grabbable.grips[bestGripIndex];

            bestGrip.inUse = true;
            physHand.goingTo = grabbing;
            physHand.gripIndex = bestGripIndex;
            physHand.oldFollowHand = physHand.follow;
            physHand.follow = FollowHand::None;

            auto& d6 = registry.emplace<worlds::D6Joint>(hand);
            d6.pxJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
            d6.setTarget(grabbing, registry);
            setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eFREE);
            logMsg("heading to grip point");

            worlds::DynamicPhysicsActor& dpa = registry.get<worlds::DynamicPhysicsActor>(hand);
            dpa.layer = worlds::NOCOLLISION_PHYSICS_LAYER;
            worlds::updatePhysicsShapes(dpa);
        } else {
            if (registry.has<worlds::ScriptComponent>(grabbing)) {
                interfaces.scriptEngine->fireEvent(grabbing, "onGrab");
            }

            Transform relativeTransform = grabbingTransform.transformByInverse(handTransform);

            auto& d6 = registry.emplace<worlds::D6Joint>(hand);
            d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, worlds::glm2px(relativeTransform));
            setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
            d6.setTarget(grabbing, registry);

            worlds::DynamicPhysicsActor& dpa = registry.get<worlds::DynamicPhysicsActor>(hand);
            auto* otherDpa = registry.try_get<worlds::DynamicPhysicsActor>(grabbing);

            if (otherDpa && useTensorCompensation.getInt()) {
                setPhysHandTensor(physHand, dpa, *otherDpa, worlds::glm2px(handTransform), grabbingTransform, registry);
                physHand.useOverrideIT = true;
            }

            logMsg("grabbed object without grip point");
        }
    }
}
