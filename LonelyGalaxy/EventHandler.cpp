#include "EventHandler.hpp"
#include <Util/RichPresence.hpp>
#include <openvr.h>
#include <Core/Log.hpp>
#include <Render/Render.hpp>
#include "Core/AssetDB.hpp"
#include "DebugArrow.hpp"
#include "Physics/D6Joint.hpp"
#include "Physics/PhysicsActor.hpp"
#include <physx/PxRigidDynamic.h>
#include "Core/Transform.hpp"
#include <VR/OpenVRInterface.hpp>
#include <Physics/Physics.hpp>
#include <Core/Console.hpp>
#include <ImGui/imgui.h>
#include <Util/MatUtil.hpp>
#include <Core/Engine.hpp>
#include "Core/NameComponent.hpp"
#include "LocospherePlayerSystem.hpp"
#include "PhysHandSystem.hpp"
#include <enet/enet.h>
#include <Core/JobSystem.hpp>
#include "Util/CreateModelObject.hpp"
#include "ObjectParentSystem.hpp"
#include "extensions/PxD6Joint.h"
#ifdef DISCORD_RPC
#include <core.h>
#endif
#include "Util/VKImGUIUtil.hpp"
#include <Scripting/ScriptComponent.hpp>
#include <Physics/FilterEntities.hpp>
#include <physxit.h>
#include "MathsUtil.hpp"
#include "PlayerStartPoint.hpp"
#include "RPGStats.hpp"
#include <Scripting/WrenVM.hpp>
#include "GripPoint.hpp"
#include <Input/Input.hpp>
#include <Physics/FixedJoint.hpp>
#include "PhysicsSoundComponent.hpp"
#include <Audio/Audio.hpp>

namespace lg {

    struct SyncedRB {};

    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    void setAllAxisD6Motion(physx::PxD6Joint* j, physx::PxD6Motion::Enum motion) {
        j->setMotion(physx::PxD6Axis::eX, motion);
        j->setMotion(physx::PxD6Axis::eY, motion);
        j->setMotion(physx::PxD6Axis::eZ, motion);
        j->setMotion(physx::PxD6Axis::eSWING1, motion);
        j->setMotion(physx::PxD6Axis::eSWING2, motion);
        j->setMotion(physx::PxD6Axis::eTWIST, motion);
    }

    EventHandler::EventHandler(bool dedicatedServer)
        : isDedicated {dedicatedServer}
        , client {nullptr}
        , server {nullptr}
        , lHandEnt {entt::null}
        , rHandEnt {entt::null} {
    }

    void EventHandler::onPhysicsSoundContact(entt::entity thisEnt, const worlds::PhysicsContactInfo& info) {
        auto& t = reg->get<Transform>(thisEnt);
        auto& physSound = reg->get<PhysicsSoundComponent>(thisEnt);
        worlds::AudioSystem::getInstance()->playOneShotClip(physSound.soundId, t.position, true, glm::min(info.relativeSpeed * 0.125f, 1.0f));
    }

    void EventHandler::onPhysicsSoundConstruct(entt::registry& reg, entt::entity ent) {
        auto& physEvents = reg.get_or_emplace<worlds::PhysicsEvents>(ent);
        physEvents.onContact = std::bind(&EventHandler::onPhysicsSoundContact, 
                this, std::placeholders::_1, std::placeholders::_2);
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;
        engine = interfaces.engine;
        scriptEngine = interfaces.scriptEngine;
        reg = &registry;

        registry.on_construct<PhysicsSoundComponent>().connect<&EventHandler::onPhysicsSoundConstruct>(this);

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });

        if (enet_initialize() != 0) {
            logErr("Failed to initialize enet.");
        }

        mpManager = new MultiplayerManager{registry, isDedicated};

        new DebugArrows(registry);

        if (vrInterface) {
            worlds::g_console->registerCommand([&](void*, const char*) {
                auto& wActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);
                auto* body = static_cast<physx::PxRigidBody*>(wActor.actor);
                body->setLinearVelocity(physx::PxVec3{ 0.0f });

                auto& lTf = registry.get<Transform>(lHandEnt);
                auto& lPh = registry.get<PhysHand>(lHandEnt);
                auto lPose = body->getGlobalPose();
                lPose.p = worlds::glm2px(lPh.targetWorldPos);
                lTf.position = lPh.targetWorldPos;
                body->setGlobalPose(lPose);

                auto& wActorR = registry.get<worlds::DynamicPhysicsActor>(rHandEnt);
                auto* rBody = static_cast<physx::PxRigidBody*>(wActorR.actor);
                rBody->setLinearVelocity(physx::PxVec3{ 0.0f });

                auto& rTf = registry.get<Transform>(rHandEnt);
                auto& rPh = registry.get<PhysHand>(rHandEnt);
                auto rPose = rBody->getGlobalPose();
                rPose.p = worlds::glm2px(rPh.targetWorldPos);
                rTf.position = rPh.targetWorldPos;
                rBody->setGlobalPose(rPose);

                lPh.posController.reset();
                lPh.rotController.reset();

                rPh.posController.reset();
                rPh.rotController.reset();
            }, "cnvrg_resetHands", "Resets hand PID controllers.", nullptr);
        }
    }

    void EventHandler::preSimUpdate(entt::registry&, float) {
        g_dbgArrows->newFrame();
    }

    entt::entity fakeLHand;
    entt::entity fakeRHand;

    void EventHandler::update(entt::registry& reg, float deltaTime, float) {
        if (vrInterface) {
            static float yRot = 0.0f;
            static float targetYRot = 0.0f;
            static bool rotated = false;
            auto rStickInput = vrInterface->getActionV2(rStick);
            auto rotateInput = rStickInput.x;
            ImGui::Text("s: %.3f, %.3f", rStickInput.x, rStickInput.y);

            float threshold = 0.5f;
            bool rotatingNow = glm::abs(rotateInput) > threshold;

            if (rotatingNow && !rotated) {
                targetYRot += glm::radians(45.0f) * -glm::sign(rotateInput);
            }

            const float rotateSpeed = 15.0f;

            yRot += glm::clamp((targetYRot - yRot), -deltaTime * rotateSpeed, deltaTime * rotateSpeed);

            camera->rotation = glm::quat{glm::vec3{0.0f, yRot, 0.0f}};

            rotated = rotatingNow;
        }

        if (reg.view<RPGStats>().size() > 0) {
            auto& rpgStat = reg.get<RPGStats>(reg.view<RPGStats>()[0]);
            static worlds::ConVar dbgRpgStats { "lg_dbgStats", "0", "Show debug menu for stat components." };
            if (ImGui::Begin("RPG Stats")) {
                ImGui::DragScalar("maxHP", ImGuiDataType_U64, &rpgStat.maxHP, 1.0f);
                ImGui::DragScalar("currentHP", ImGuiDataType_U64, &rpgStat.currentHP, 1.0f);
                ImGui::DragScalar("level", ImGuiDataType_U64, &rpgStat.level, 1.0f);
                ImGui::DragScalar("totalExperience", ImGuiDataType_U64, &rpgStat.totalExperience, 1.0f);
                ImGui::DragScalar("strength", ImGuiDataType_U8, &rpgStat.strength, 1.0f);
            }
            ImGui::End();

            auto drawList = ImGui::GetBackgroundDrawList();

            std::string healthText = "health: " + std::to_string(rpgStat.currentHP) + "/" + std::to_string(rpgStat.maxHP);
            auto viewSize = ImGui::GetIO().DisplaySize;
            drawList->AddText(ImVec2(15, viewSize.y - 30), ImColor(1.0f, 1.0f, 1.0f), healthText.c_str());

            if (reg.valid(lHandEnt) && reg.valid(rHandEnt)) {
                auto& phl = reg.get<PhysHand>(lHandEnt);
                auto& phr = reg.get<PhysHand>(rHandEnt);

                float forceLimit = 150.0f + (100.0f * rpgStat.strength);
                float torqueLimit = 7.0f + (10.0f * rpgStat.strength);

                if (!reg.valid(phl.goingTo)) {
                    phl.forceLimit = forceLimit;
                    phr.forceLimit = forceLimit;
                }

                if (!reg.valid(phr.goingTo)) {
                    phl.torqueLimit = torqueLimit;
                    phr.torqueLimit = torqueLimit;
                }

                if (reg.valid(fakeLHand) && reg.valid(fakeRHand)) {
                    auto& tfl = reg.get<Transform>(fakeLHand);
                    auto& trl = reg.get<Transform>(fakeRHand);

                    tfl.position = phl.targetWorldPos;
                    tfl.rotation = phl.targetWorldRot;

                    trl.position = phr.targetWorldPos;
                    trl.rotation = phr.targetWorldRot;
                }
            }
        }
    }

    int syncTimer = 0;
    worlds::ConVar itCompDbg { "lg_itCompDbg", "0", "Shows physics shapes for grabbed objects." };

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

    worlds::ConVar useTensorCompensation{"lg_compensateTensors", "1", "Enables inertia tensor compensation on grabs."};
    worlds::ConVar enableGripPoints { "lg_enableGripPoints", "1", "Enables grip points." };

    extern void resetHand(PhysHand& ph, physx::PxRigidBody* rb);

    void EventHandler::updateHandGrab(entt::registry& registry, PlayerRig& rig, entt::entity ent, float deltaTime) {
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

            if (distance < 0.002f && rotDot > 0.99f) {
                auto& d6 = registry.get<worlds::D6Joint>(ent);
                d6.setTarget(physHand.goingTo, registry);

                logMsg("hit target, locking joint for grab");
                physHand.useOverrideIT = true;
                physHand.goingTo = entt::null;

                physx::PxTransform target {
                    worlds::glm2px(-gripPoint.offset),
                    worlds::glm2px(glm::normalize(gripPoint.rotOffset))
                };

                auto handT = dpa.actor->getGlobalPose();
                auto objectT = otherActor.actor->getGlobalPose();
                d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, handT.transformInv(objectT));
                d6.pxJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
                setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
                if (useTensorCompensation.getInt()) {
                    setPhysHandTensor(physHand, dpa, otherActor, worlds::glm2px(handTf), otherTf, *reg);
                }

                physHand.useOverrideIT = true;
                physHand.follow = physHand.oldFollowHand;
                physHand.forceMultiplier = 1.0f;
                otherActor.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
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
                        scriptEngine->fireEvent(pickUp, "onGrab");
                    }

                    Transform otherTf = worlds::px2glm(touch.actor->getGlobalPose());
                    otherTf.scale = registry.get<Transform>(pickUp).scale;
                    auto* gripPoint = registry.try_get<GripPoint>(pickUp);

                    //auto& fj = registry.emplace<worlds::FixedJoint>(ent);
                    physx::PxTransform p2 = touch.actor->getGlobalPose();
                    logMsg("grabbing object");

                    if (enableGripPoints.getInt() && gripPoint && (!gripPoint->exclusive || !gripPoint->currentlyHeld)) {
                        Transform handTarget;
                        handTarget.position = otherTf.position + (otherTf.rotation * gripPoint->offset);
                        handTarget.rotation = otherTf.rotation * gripPoint->rotOffset;
                        gripPoint->currentlyHeld = true;
                        physHand.goingTo = pickUp;
                        physHand.oldFollowHand = physHand.follow;
                        physHand.follow = FollowHand::None;
                        //physHand.targetWorldPos = handTarget.position;
                        //physHand.targetWorldRot = handTarget.rotation;
                        auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                        d6.setTarget(pickUp, registry);
                        setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eFREE);
                        logMsg("heading to grip point");
                        //touch.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
                    } else {
                        auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                        d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, t.transformInv(p2));
                        setAllAxisD6Motion(d6.pxJoint, physx::PxD6Motion::eLOCKED);
                        d6.setTarget(pickUp, registry);
                        // mass of hands is 2kg
                        auto* otherDpa = registry.try_get<worlds::DynamicPhysicsActor>(pickUp);

                        if (otherDpa && useTensorCompensation.getInt()) {
                            setPhysHandTensor(physHand, dpa, *otherDpa, t, otherTf, *reg);
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
            GripPoint* gp = registry.try_get<GripPoint>(heldEnt);
            if (gp)
                gp->currentlyHeld = false;
            registry.remove<worlds::D6Joint>(ent);
            auto& ph = registry.get<PhysHand>(ent);
            ph.useOverrideIT = false;
            ph.forceMultiplier = 1.0f;
        }
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
        mpManager->simulate(simStep);

        entt::entity localLocosphereEnt = entt::null;
        LocospherePlayerComponent* localLpc = nullptr;

        registry.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!registry.valid(localLocosphereEnt)) {
                    localLocosphereEnt = ent;
                    localLpc = &lpc;
                } else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!registry.valid(localLocosphereEnt)) {
            // probably dedicated server ¯\_(ツ)_/¯
            return;
        }

        auto& localRig = registry.get<PlayerRig>(localLocosphereEnt);

        updateHandGrab(registry, localRig, localRig.lHand, simStep);
        updateHandGrab(registry, localRig, localRig.rHand, simStep);

        if (vrInterface) {
            float fenderHeight = 0.55f;
            glm::vec3 headPos = worlds::getMatrixTranslation(vrInterface->getHeadTransform());

            rHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                    physx::PxVec3(0.0f, headPos.y - fenderHeight, 0.0f), physx::PxQuat { physx::PxIdentity }});
            lHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                    physx::PxVec3(0.0f, headPos.y - fenderHeight, 0.0f), physx::PxQuat { physx::PxIdentity }});
            ImGui::Text("Headpos: %.3f, %.3f, %.3f", headPos.x, headPos.y, headPos.z);
        }
    }

    worlds::ConVar showTargetHands { "lg_showTargetHands", "0", "Shows devtextured hands that represent the current target transform of the hands." };

    void EventHandler::onSceneStart(entt::registry& registry) {
        registry.view<worlds::DynamicPhysicsActor>().each([&](auto ent, auto&) {
            registry.emplace<SyncedRB>(ent);
        });

        // create our lil' pal the player
        if (!isDedicated && registry.view<PlayerStartPoint>().size() > 0) {
            entt::entity pspEnt = registry.view<PlayerStartPoint, Transform>().front();
            Transform& pspTf = registry.get<Transform>(pspEnt);

            PlayerRig rig = lsphereSys->createPlayerRig(registry, pspTf.position);
            auto& lpc = registry.get<LocospherePlayerComponent>(rig.locosphere);
            lpc.isLocal = true;
            lpc.sprint = false;
            lpc.maxSpeed = 0.0f;
            lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);
            auto& stats = registry.emplace<RPGStats>(rig.locosphere);
            stats.strength = 15;

            logMsg("Created player rig");

            if (vrInterface) {
                lGrab = vrInterface->getActionHandle("/actions/main/in/GrabL");
                rGrab = vrInterface->getActionHandle("/actions/main/in/GrabR");
                rStick = vrInterface->getActionHandle("/actions/main/in/RStick");
                camera->rotation = glm::quat{};
            }

            auto& fenderTransform = registry.get<Transform>(rig.fender);
            auto matId = worlds::g_assetDB.addOrGetExisting("Materials/VRHands/placeholder.json");
            auto devMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
            auto lHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_l.wmdl");
            auto rHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_r.wmdl");

            lHandEnt = registry.create();
            registry.get<PlayerRig>(rig.locosphere).lHand = lHandEnt;
            registry.emplace<worlds::WorldObject>(lHandEnt, matId, lHandModel);
            auto& lht = registry.emplace<Transform>(lHandEnt);
            lht.position = glm::vec3(0.5, 0.0f, 0.0f) + fenderTransform.position;
            registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

            if (showTargetHands.getInt()) {
                fakeLHand = registry.create();
                registry.emplace<worlds::WorldObject>(fakeLHand, devMatId, lHandModel);
                registry.emplace<Transform>(fakeLHand);
                registry.emplace<worlds::NameComponent>(fakeLHand).name = "Fake L. Handy";
            }

            rHandEnt = registry.create();
            registry.get<PlayerRig>(rig.locosphere).rHand = rHandEnt;
            registry.emplace<worlds::WorldObject>(rHandEnt, matId, rHandModel);
            auto& rht = registry.emplace<Transform>(rHandEnt);
            rht.position = glm::vec3(-0.5f, 0.0f, 0.0f) + fenderTransform.position;
            registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

            if (showTargetHands.getInt()) {
                fakeRHand = registry.create();
                registry.emplace<worlds::WorldObject>(fakeRHand, devMatId, rHandModel);
                registry.emplace<Transform>(fakeRHand);
                registry.emplace<worlds::NameComponent>(fakeRHand).name = "Fake R. Handy";
            }

            auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            // Using the reference returned by this doesn't work unfortunately.
            registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

            auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
            auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

            rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.045f, 0.05f }));
            rwActor.physicsShapes[0].pos = glm::vec3{0.0f, 0.0f, 0.03f};
            lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.045f, 0.05f }));
            lwActor.physicsShapes[0].pos = glm::vec3{0.0f, 0.0f, 0.03f};

            rwActor.layer = worlds::PLAYER_PHYSICS_LAYER;
            lwActor.layer = worlds::PLAYER_PHYSICS_LAYER;

            worlds::updatePhysicsShapes(rwActor);
            worlds::updatePhysicsShapes(lwActor);

            rActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
            lActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);

            worlds::g_scene->addActor(*rActor);
            worlds::g_scene->addActor(*lActor);

            physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
            physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

            PIDSettings posSettings{ 750.0f, 600.0f, 137.0f };
            PIDSettings rotSettings{ 200.0f, 0.0f, 29.0f };

            auto& lHandPhys = registry.emplace<PhysHand>(lHandEnt);
            lHandPhys.locosphere = rig.locosphere;
            lHandPhys.posController.acceptSettings(posSettings);
            lHandPhys.posController.averageAmount = 5.0f;
            lHandPhys.rotController.acceptSettings(rotSettings);
            lHandPhys.rotController.averageAmount = 2.0f;
            lHandPhys.follow = FollowHand::LeftHand;

            auto& rHandPhys = registry.emplace<PhysHand>(rHandEnt);

            rHandPhys.locosphere = rig.locosphere;
            rHandPhys.posController.acceptSettings(posSettings);
            rHandPhys.posController.averageAmount = 5.0f;
            rHandPhys.rotController.acceptSettings(rotSettings);
            rHandPhys.rotController.averageAmount = 2.0f;
            rHandPhys.follow = FollowHand::RightHand;

            auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(rig.fender).actor;

            lHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, lActor,
            physx::PxTransform { physx::PxIdentity });

            lHandJoint->setLinearLimit(physx::PxJointLinearLimit{
                    physx::PxTolerancesScale{}, 0.8f});
            lHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
            lHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
            lHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

            rHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, rActor,
            physx::PxTransform { physx::PxIdentity });

            rHandJoint->setLinearLimit(physx::PxJointLinearLimit{
                    physx::PxTolerancesScale{}, 0.8f});
            rHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
            rHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
            rHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
            lActor->setSolverIterationCounts(16, 8);
            rActor->setSolverIterationCounts(16, 8);
            lActor->setLinearVelocity(physx::PxVec3{0.0f});
            rActor->setLinearVelocity(physx::PxVec3{0.0f});

            rHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                physx::PxVec3 { 0.0f, 0.8f, 0.0f },
                physx::PxQuat { physx::PxIdentity }
            });

            lHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                physx::PxVec3 { 0.0f, 0.8f, 0.0f },
                physx::PxQuat { physx::PxIdentity }
            });
        }

        if (isDedicated) {
            mpManager->onSceneStart(registry);
        }

        g_dbgArrows->createEntities();
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (registry.valid(lHandEnt)) {
            registry.destroy(lHandEnt);
            registry.destroy(rHandEnt);
        }

        if (client)
            delete client;

        if (server)
            delete server;

        enet_deinitialize();
    }
}
