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
                        worlds::g_assetDB.addOrGetExisting(shape.type == worlds::PhysicsShapeType::Sphere ?
                            "uvsphere.obj" : "model.obj"),
                        worlds::g_assetDB.addOrGetExisting("Materials/dev.json"),
                        shape.type == worlds::PhysicsShapeType::Sphere ?
                        glm::vec3{shape.sphere.radius * 0.5f} :
                        shape.box.halfExtents * scale);
            } else {
                worlds::createModelObject(reg, worlds::px2glm(wsTransform.p), worlds::px2glm(wsTransform.q),
                        worlds::g_assetDB.addOrGetExisting(shape.type == worlds::PhysicsShapeType::Sphere ?
                            "uvsphere.obj" : "model.obj"),
                        worlds::g_assetDB.addOrGetExisting("Materials/dev.json"),
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
            const physx::PxTransform& handT, Transform& objectT,
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
        static V3PidController objPid;
        objPid.P = 35.0f;
        objPid.D = 7.0f;
        objPid.I = 1.0f;
        objPid.averageAmount = 5.0f;
        auto& physHand = registry.get<PhysHand>(ent);
        auto grabAction = physHand.follow == FollowHand::LeftHand ? lGrab : rGrab;
        auto grabButton = physHand.follow == FollowHand::LeftHand ? worlds::MouseButton::Left : worlds::MouseButton::Right;
        bool doGrab = vrInterface ? vrInterface->getActionPressed(grabAction) : inputManager->mouseButtonPressed(grabButton);
        bool doRelease = vrInterface ? vrInterface->getActionReleased(grabAction) : inputManager->mouseButtonReleased(grabButton);
        auto& handTf = registry.get<Transform>(ent);
        auto& dpa = registry.get<worlds::DynamicPhysicsActor>(ent);

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
            auto& gripPoint = registry.get<GripPoint>(physHand.goingTo);

            glm::vec3 targetHandPos = otherTf.position + (otherTf.rotation * gripPoint.offset);
            glm::quat targetHandRot = otherTf.rotation * gripPoint.rotOffset;
            float distance = glm::distance(handTf.position, targetHandPos);

            if (distance > 0.5f) {
                logMsg("too far");
                physHand.goingTo = entt::null;
                physHand.follow = physHand.oldFollowHand;
                registry.remove_if_exists<worlds::D6Joint>(ent);
                otherActor.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
                return;
            }

            float rotDot = glm::dot(fixupQuat(targetHandRot), fixupQuat(handTf.rotation));
            ImGui::Text("%.3f distance, %.3f rotDot", distance, rotDot);
            glm::vec3 displacement = targetHandPos - handTf.position;
            ImGui::Text("displacement: %.3f, %.3f, %.3f", displacement.x, displacement.y, displacement.z);

            physHand.targetWorldPos = targetHandPos;
            physHand.targetWorldRot = targetHandRot;

            glm::vec3 offset = gripPoint.offset;
            offset = glm::inverse(gripPoint.rotOffset) * offset;
            offset = handTf.rotation * offset;

            glm::vec3 targetObjPos = handTf.position - offset;
            glm::quat targetObjRot = handTf.rotation * gripPoint.rotOffset;

            glm::vec3 pidOut = objPid.getOutput(targetObjPos - otherTf.position, deltaTime);

            //otherActor.actor->addForce(worlds::glm2px(pidOut), physx::PxForceMode::eACCELERATION);

            if (distance < 0.005f && rotDot > 0.9f) {
                auto& d6 = registry.get<worlds::D6Joint>(ent);
                d6.setTarget(physHand.goingTo, registry);

                logMsg("hit target, locking joint for grab");
                physHand.useOverrideIT = true;
                physHand.goingTo = entt::null;

                physx::PxTransform target {
                    worlds::glm2px(gripPoint.offset),
                    worlds::glm2px(glm::normalize(gripPoint.rotOffset))
                };

                auto handT = dpa.actor->getGlobalPose();
                auto objectT = otherActor.actor->getGlobalPose();
                //d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, handT.transformInv(objectT));
                d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, target);
                d6.pxJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
                setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
                if (useTensorCompensation.getInt()) {
                    setPhysHandTensor(physHand, dpa, otherActor, worlds::glm2px(handTf), otherTf, registry);
                }

                physHand.useOverrideIT = true;
                physHand.follow = physHand.oldFollowHand;
                physHand.forceMultiplier = 1.0f;
                otherActor.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);

                dpa.layer = worlds::PLAYER_PHYSICS_LAYER;
                worlds::updatePhysicsShapes(dpa);
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

            worlds::FilterEntities filterEnt;
            filterEnt.ents[0] = (uint32_t)rig.lHand;
            filterEnt.ents[1] = (uint32_t)rig.rHand;
            filterEnt.ents[2] = (uint32_t)rig.locosphere;
            filterEnt.ents[3] = (uint32_t)rig.fender;
            filterEnt.ents[4] = (uint32_t)ent;
            filterEnt.numFilterEnts = 5;
            auto t = dpa.actor->getGlobalPose();
            auto overlapCenter = t;
            overlapCenter.p += t.q.rotate(worlds::glm2px(dpa.physicsShapes[0].pos));

            if (worlds::g_scene->overlap(sphereGeo, overlapCenter, hit, filterData, &filterEnt)) {
                const auto& touch = hit.getAnyHit(0);
                auto pickUp = (entt::entity)(uint32_t)(uintptr_t)touch.actor->userData;

                if (registry.valid(pickUp) && registry.valid(ent)) {
                    if (registry.has<worlds::ScriptComponent>(pickUp)) {
                        interfaces.scriptEngine->fireEvent(pickUp, "onGrab");
                    }

                    Transform otherTf = worlds::px2glm(touch.actor->getGlobalPose());
                    otherTf.scale = registry.get<Transform>(pickUp).scale;
                    auto* gripPoint = registry.try_get<GripPoint>(pickUp);

                    //auto& fj = registry.emplace<worlds::FixedJoint>(ent);
                    physx::PxTransform p2 = touch.actor->getGlobalPose();
                    logMsg("grabbing object");

                    if (enableGripPoints.getInt() && gripPoint && (!gripPoint->exclusive || !gripPoint->currentlyHeld)) {
                        gripPoint->currentlyHeld = true;
                        physHand.goingTo = pickUp;
                        physHand.oldFollowHand = physHand.follow;
                        physHand.follow = FollowHand::None;

                        auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                        d6.pxJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
                        d6.setTarget(pickUp, registry);
                        setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eFREE);
                        logMsg("heading to grip point");
                        dpa.layer = worlds::NOCOLLISION_PHYSICS_LAYER;
                        worlds::updatePhysicsShapes(dpa);

                        //touch.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
                    } else {
                        auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                        d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, t.transformInv(p2));
                        setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
                        d6.setTarget(pickUp, registry);
                        // mass of hands is 2kg
                        auto* otherDpa = registry.try_get<worlds::DynamicPhysicsActor>(pickUp);

                        if (otherDpa && useTensorCompensation.getInt()) {
                            setPhysHandTensor(physHand, dpa, *otherDpa, t, otherTf, registry);
                            physHand.useOverrideIT = true;
                        }

                        logMsg("grabbed object without grip point");
                    }
                }
            }
        }

        if (doRelease && registry.has<worlds::D6Joint>(ent)) {
            auto& d6 = registry.get<worlds::D6Joint>(ent);
            auto heldEnt = d6.getTarget();

            if (registry.valid(heldEnt)) {
                GripPoint* gp = registry.try_get<GripPoint>(heldEnt);
                if (gp)
                    gp->currentlyHeld = false;
            }

            registry.remove<worlds::D6Joint>(ent);
            auto& ph = registry.get<PhysHand>(ent);
            ph.useOverrideIT = false;
            ph.forceMultiplier = 1.0f;
        }
    }
}
