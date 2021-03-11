#include "EventHandler.hpp"
#include <Util/RichPresence.hpp>
#include <openvr.h>
#include <Core/Log.hpp>
#include <Render/Render.hpp>
#include "Core/AssetDB.hpp"
#include "DebugArrow.hpp"
#include "NetMessage.hpp"
#include "Physics/D6Joint.hpp"
#include "Physics/PhysicsActor.hpp"
#include <physx/PxRigidDynamic.h>
#include "Render/Loaders/SourceModelLoader.hpp"
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

namespace lg {
    const uint16_t CONVERGE_PORT = 3011;

    struct SyncedRB {};

    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer)
        : isDedicated {dedicatedServer}
        , client {nullptr}
        , server {nullptr}
        , lHandEnt {entt::null}
        , rHandEnt {entt::null} {
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;
        engine = interfaces.engine;
        scriptEngine = interfaces.scriptEngine;
        reg = &registry;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });

        if (enet_initialize() != 0) {
            logErr("Failed to initialize enet.");
        }

        if (isDedicated) {
            server = new Server{};
            server->setCallbackCtx(this);
            server->setConnectionCallback(onPlayerJoin);
            server->setDisconnectionCallback(onPlayerLeave);
            server->start();
        } else {
            client = new Client{};
            client->setCallbackCtx(this);

            worlds::g_console->registerCommand([&](void*, const char*) {
                if (!client->serverPeer ||
                        client->serverPeer->state != ENET_PEER_STATE_CONNECTED) {
                    logErr("not connected!");
                    return;
                }

                client->disconnect();
            }, "disconnect", "Disconnect from the server.", nullptr);

            worlds::g_console->registerCommand([&](void*, const char* arg) {
                if (client->isConnected()) {
                    logErr("already connected! disconnect first.");
                }

                // assume the argument is an address
                ENetAddress addr;
                enet_address_set_host(&addr, arg);
                addr.port = CONVERGE_PORT;

                client->connect(addr);
            }, "connect", "Connects to the specified server.", nullptr);
        }

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

    void EventHandler::update(entt::registry& reg, float, float) {
        if (reg.view<RPGStats>().size() > 0) {
            auto& rpgStat = reg.get<RPGStats>(reg.view<RPGStats>()[0]);
            if (ImGui::Begin("RPG Stats")) {
                ImGui::DragScalar("maxHP", ImGuiDataType_U64, &rpgStat.maxHP, 1.0f);
                ImGui::DragScalar("currentHP", ImGuiDataType_U64, &rpgStat.currentHP, 1.0f);
                ImGui::DragScalar("level", ImGuiDataType_U64, &rpgStat.level, 1.0f);
                ImGui::DragScalar("totalExperience", ImGuiDataType_U64, &rpgStat.totalExperience, 1.0f);
                ImGui::DragScalar("strength", ImGuiDataType_U8, &rpgStat.strength, 1.0f);
            }
            ImGui::End();

            if (reg.valid(lHandEnt) && reg.valid(rHandEnt)) {
                auto& phl = reg.get<PhysHand>(lHandEnt);
                auto& phr = reg.get<PhysHand>(rHandEnt);

                float forceLimit = 100 + (150 * rpgStat.strength);
                float torqueLimit = 2 + (5 * rpgStat.strength);

                phl.forceLimit = forceLimit;
                phr.forceLimit = forceLimit;

                phl.torqueLimit = torqueLimit;
                phr.torqueLimit = torqueLimit;
            }
        }

        if (client) {
            client->processMessages(onClientPacket);

#ifdef DISCORD_RPC
            if (!setClientInfo && worlds::discordCore) {
                discord::User currUser;
                auto res = worlds::discordCore->UserManager().GetCurrentUser(&currUser);

                if (res == discord::Result::Ok) {
                    logMsg("got user info, setting client info for %s#%s with id %lu...", currUser.GetUsername(), currUser.GetDiscriminator(), currUser.GetId());
                    client->setClientInfo(1, currUser.GetId(), 1);
                    setClientInfo = true;
                }
            }
#endif
            if (client->isConnected()) {
                ImGui::Begin("netdbg");
                ImVec2 cr = ImGui::GetContentRegionAvail();
                ImGui::PlotLines("err", lsphereErr, 128, lsphereErrIdx, nullptr, FLT_MAX, FLT_MAX, ImVec2(cr.x - 10.0f, 100.0f));
                uint32_t idxWrapped = lsphereErrIdx - 1;
                if (idxWrapped == ~0u)
                    idxWrapped = 127;
                ImGui::Text("curr err: %.3f", lsphereErr[idxWrapped]);
                ImGui::End();
            }
        }
    }

    worlds::ConVar sendRate {"cnvrg_sendRate", "5", "Send rate in simulation ticks. 0 = 1 packet per tick"};
    int syncTimer = 0;

    void addShapeTensor(entt::registry& reg, worlds::PhysicsShape& shape, physx::IT::InertiaTensorComputer& itComp,
            glm::vec3 shapePos, glm::quat shapeRot) {
        physx::IT::InertiaTensorComputer shapeComp(false);

        physx::PxTransform transform;
        transform.p = worlds::glm2px(shapePos);
        transform.q = worlds::glm2px(shapeRot);

        switch (shape.type) {
        case worlds::PhysicsShapeType::Sphere:
            shapeComp.setSphere(shape.sphere.radius, &transform);
            break;
        case worlds::PhysicsShapeType::Box:
            shapeComp.setBox(worlds::glm2px(shape.box.halfExtents), &transform);
            break;
        case worlds::PhysicsShapeType::Capsule:
            shapeComp.setCapsule(0, shape.capsule.radius, shape.capsule.height, &transform);
            break;
        }

        itComp.add(shapeComp);
    }

    worlds::ConVar useTensorCompensation{"lg_compensateTensors", "1", "Enables inertia tensor compensation on grabs."};

    void EventHandler::updateHandGrab(entt::registry& registry, PlayerRig& rig, entt::entity ent) {
        auto& physHand = registry.get<PhysHand>(ent);
        auto grabAction = physHand.follow == FollowHand::LeftHand ? lGrab : rGrab;

        if (vrInterface->getActionPressed(grabAction) && !registry.has<worlds::D6Joint>(ent)) {
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
            auto& dpa = registry.get<worlds::DynamicPhysicsActor>(ent);
            auto t = dpa.actor->getGlobalPose();

            if (worlds::g_scene->overlap(sphereGeo, t, hit, filterData, &filterEnt)) {
                const auto& touch = hit.getAnyHit(0);
                auto pickUp = (entt::entity)(uint32_t)(uintptr_t)touch.actor->userData;

                if (registry.has<worlds::ScriptComponent>(pickUp)) {
                    scriptEngine->fireEvent(pickUp, "onGrab");
                }

                if (registry.valid(pickUp) && registry.valid(ent)) {
                    auto& d6 = registry.emplace<worlds::D6Joint>(ent);
                    // mass of hands is 2kg
                    auto* otherDpa = registry.try_get<worlds::DynamicPhysicsActor>(pickUp);

                    if (otherDpa && useTensorCompensation.getInt()) {
                        // find offset of other physics actor
                        auto otherT = otherDpa->actor->getGlobalPose();

                        // calculate combined inertia tensor
                        physx::IT::InertiaTensorComputer itComp(true);

                        for (auto& shape : otherDpa->physicsShapes) {
                            auto worldPos = otherT.transform(worlds::glm2px(shape.pos));
                            auto worldRot = otherT.q * worlds::glm2px(shape.rot);

                            glm::vec3 handSpacePos = worlds::px2glm(t.transformInv(worldPos));
                            glm::quat handSpaceRot = worlds::px2glm(worldRot * t.q.getConjugate());
                            addShapeTensor(*reg, shape, itComp, handSpacePos, handSpaceRot);
                        }

                        for (auto& shape : dpa.physicsShapes) {
                            addShapeTensor(*reg, shape, itComp, shape.pos, shape.rot);
                        }

                        itComp.scaleDensity((dpa.mass + otherDpa->mass) / itComp.getMass());

                        physx::PxQuat itRot;
                        auto tensor = physx::PxDiagonalize(itComp.getInertia(), itRot);
                        physHand.useOverrideIT = true;
                        physHand.overrideIT = worlds::px2glm(tensor);
                        physHand.overrideITRotation = worlds::px2glm(itRot);
                        physHand.forceMultiplier = glm::max(1.0f, glm::min(15.0f, otherDpa->mass / 2.0f));
                    }

                    physx::PxTransform p = dpa.actor->getGlobalPose();
                    physx::PxTransform p2 = touch.actor->getGlobalPose();
                    d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, p.transformInv(p2));
                    d6.setTarget(pickUp, registry);
                }
            }
        }

        if (vrInterface->getActionReleased(grabAction)) {
            if (registry.has<worlds::D6Joint>(ent)) {
                registry.remove<worlds::D6Joint>(ent);
                auto& ph = registry.get<PhysHand>(ent);
                ph.useOverrideIT = false;
                ph.forceMultiplier = 1.0f;
            }
        }
    }

    void EventHandler::simulate(entt::registry& registry, float) {
        if (isDedicated) {
            server->processMessages(onServerPacket);

            registry.view<ServerPlayer, LocospherePlayerComponent>().each([](auto, ServerPlayer& sp, LocospherePlayerComponent& lpc) {
                if (sp.inputMsgs.size() == 0) return;
                auto& pi = sp.inputMsgs.front();
                lpc.xzMoveInput = pi.xzMoveInput;
                lpc.sprint = pi.sprint;
                lpc.jump |= pi.jump;
                sp.inputMsgs.pop_front();
            });

            syncTimer++;
            if (syncTimer >= sendRate.getInt()) {
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (!server->players[i].present) continue;

                    auto& sp = registry.get<ServerPlayer>(playerLocospheres[i]);
                    auto& dpa = registry.get<worlds::DynamicPhysicsActor>(playerLocospheres[i]);
                    auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                    auto pose = dpa.actor->getGlobalPose();

                    msgs::PlayerPosition pPos;
                    pPos.id = i;
                    pPos.pos = worlds::px2glm(pose.p);
                    pPos.rot = worlds::px2glm(pose.q);
                    pPos.linVel = worlds::px2glm(rd->getLinearVelocity());
                    pPos.angVel = worlds::px2glm(rd->getAngularVelocity());
                    pPos.inputIdx = sp.lastAcknowledgedInput;

                    server->broadcastPacket(pPos.toPacket(0), NetChannel_Player);
                }

                registry.view<SyncedRB, worlds::DynamicPhysicsActor>().each([&](auto ent, worlds::DynamicPhysicsActor& dpa) {
                    auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                    auto pose = dpa.actor->getGlobalPose();

                    if (rd->isSleeping()) return;

                    msgs::RigidbodySync rSync;
                    rSync.entId = (uint32_t)ent;

                    rSync.pos = worlds::px2glm(pose.p);
                    rSync.rot = worlds::px2glm(pose.q);
                    rSync.linVel = worlds::px2glm(rd->getLinearVelocity());
                    rSync.angVel = worlds::px2glm(rd->getAngularVelocity());

                    server->broadcastPacket(rSync.toPacket(0), NetChannel_World);
                });
                syncTimer = 0;
            }
        }

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

        if (client->isConnected()) {
            auto& dpa = registry.get<worlds::DynamicPhysicsActor>(localLocosphereEnt);
            if (ImGui::Begin("client dbg")) {
                ImGui::Text("curr input idx: %u", clientInputIdx);
                ImGui::Text("past locosphere state count: %zu", pastLocosphereStates.size());
            }
            ImGui::End();

            // send input to server
            msgs::PlayerInput pi;
            pi.xzMoveInput = localLpc->xzMoveInput;
            pi.sprint = localLpc->sprint;
            pi.inputIdx = clientInputIdx;
            pi.jump = localLpc->jump;
            client->sendPacketToServer(pi.toPacket(0), NetChannel_Player);

            auto pose = dpa.actor->getGlobalPose();
            auto* rd = (physx::PxRigidDynamic*)dpa.actor;

            static glm::vec3 lastVel{ 0.0f };

            pastLocosphereStates.insert({ clientInputIdx,
                {
                    worlds::px2glm(pose.p),
                    worlds::px2glm(rd->getLinearVelocity()),
                    worlds::px2glm(rd->getAngularVelocity()),
                    lastVel - worlds::px2glm(rd->getLinearVelocity()),
                    clientInputIdx
                }
            });

            lastVel = worlds::px2glm(rd->getLinearVelocity());
            clientInputIdx++;
            lastSent = pi;
        }

        if (vrInterface) {
            auto& localRig = registry.get<PlayerRig>(localLocosphereEnt);

            updateHandGrab(registry, localRig, localRig.lHand);
            updateHandGrab(registry, localRig, localRig.rHand);
        }
    }

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
            registry.emplace<RPGStats>(rig.locosphere);

            if (vrInterface) {
                lGrab = vrInterface->getActionHandle("/actions/main/in/GrabL");
                rGrab = vrInterface->getActionHandle("/actions/main/in/GrabR");
                auto& fenderTransform = registry.get<Transform>(rig.fender);
                this->camera->rotation = glm::quat{};
                auto matId = worlds::g_assetDB.addOrGetExisting("Materials/VRHands/placeholder.json");
                //auto saberId = worlds::g_assetDB.addOrGetExisting("saber.wmdl");
                auto lHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_l.wmdl");
                auto rHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_r.wmdl");

                lHandEnt = registry.create();
                registry.get<PlayerRig>(rig.locosphere).lHand = lHandEnt;
                auto& lhWO = registry.emplace<worlds::WorldObject>(lHandEnt, matId, lHandModel);
                auto& lht = registry.emplace<Transform>(lHandEnt);
                lht.position = glm::vec3(0.5, 0.0f, 0.0f) + fenderTransform.position;
                registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

                rHandEnt = registry.create();
                registry.get<PlayerRig>(rig.locosphere).rHand = rHandEnt;
                auto& rhWO = registry.emplace<worlds::WorldObject>(rHandEnt, matId, rHandModel);
                auto& rht = registry.emplace<Transform>(rHandEnt);
                rht.position = glm::vec3(-0.5f, 0.0f, 0.0f) + fenderTransform.position;
                registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

                auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                // Using the reference returned by this doesn't work unfortunately.
                registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

                auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
                auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

                rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.05f, 0.05f }));
                lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.05f, 0.05f }));

                worlds::updatePhysicsShapes(rwActor);
                worlds::updatePhysicsShapes(lwActor);

                worlds::g_scene->addActor(*rActor);
                worlds::g_scene->addActor(*lActor);

                physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
                physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

                PIDSettings posSettings{ 750.0f, 638.0f, 137.0f };
                PIDSettings rotSettings{ 21.0f, 100.0f, 18.0f };

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
                lHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.5f});
                lHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

                rHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, rActor,
                physx::PxTransform { physx::PxIdentity });
                rHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.5f});
                rHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
            }
        }

        if (isDedicated) {
            msgs::SetScene setScene;
            setScene.sceneName = engine->getCurrentSceneInfo().name;
            server->broadcastPacket(
                    setScene.toPacket(ENET_PACKET_FLAG_RELIABLE),
                    NetChannel_Default);

            // recreate player locospheres
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!server->players[i].present) return;
                PlayerRig newRig = lsphereSys->createPlayerRig(*reg);
                auto& lpc = reg->get<LocospherePlayerComponent>(newRig.locosphere);
                lpc.isLocal = false;
                auto& sp = reg->emplace<ServerPlayer>(newRig.locosphere);
                sp.lastAcknowledgedInput = 0;
                playerLocospheres[i] = newRig.locosphere;
            }
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

    void EventHandler::onServerPacket(const ENetEvent& evt, void* vp) {
        auto* packet = evt.packet;
        EventHandler* _this = (EventHandler*)vp;

        if (packet->data[0] == MessageType::PlayerInput) {
            msgs::PlayerInput pi;
            pi.fromPacket(packet);

            // send it to the proper locosphere!
            uint8_t idx = (uintptr_t)evt.peer->data;
            entt::entity locosphereEnt = _this->playerLocospheres[idx];

            auto& sp = _this->reg->get<ServerPlayer>(locosphereEnt);
            sp.inputMsgs.push_back(pi);
            sp.lastAcknowledgedInput = pi.inputIdx;
        }
    }

    void EventHandler::onClientPacket(const ENetEvent& evt, void* vp) {
        EventHandler* _this = (EventHandler*)vp;

        if (evt.packet->data[0] == MessageType::PlayerPosition) {
            msgs::PlayerPosition pPos;
            pPos.fromPacket(evt.packet);

            if (pPos.id == _this->client->serverSideID) {
                _this->reg->view<LocospherePlayerComponent, worlds::DynamicPhysicsActor, Transform>()
                    .each([&](auto, LocospherePlayerComponent& lpc, worlds::DynamicPhysicsActor& dpa, Transform& t) {
                    if (lpc.isLocal) {
                        auto pose = dpa.actor->getGlobalPose();
                        auto pastStateIt = _this->pastLocosphereStates.find(pPos.inputIdx);

                        if (pastStateIt != _this->pastLocosphereStates.end()) {
                            auto pastState = _this->pastLocosphereStates.at(pPos.inputIdx);
                            float err = glm::length(pastState.pos - pPos.pos);

                            _this->lsphereErr[_this->lsphereErrIdx] = err;
                            _this->lsphereErrIdx++;

                            if (_this->lsphereErrIdx == 128)
                                _this->lsphereErrIdx = 0;
                        }

                        std::erase_if(_this->pastLocosphereStates, [&](auto& k) {
                            return k.first <= pPos.inputIdx;
                        });

                        //if (glm::distance(pastState.pos, pPos.pos) > 0.25f) {
                            //logMsg("correcting");
                        pose.p = worlds::glm2px(pPos.pos);
                        pose.q = worlds::glm2px(pPos.rot);
                        auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                        glm::vec3 linVel = pPos.linVel;
                        //rd->setLinearVelocity(worlds::glm2px(pPos.linVel));
                        //rd->setAngularVelocity(worlds::glm2px(pPos.angVel));

                        for (auto& p : _this->pastLocosphereStates) {
                            pose.p += worlds::glm2px(linVel * 0.01f);
                            linVel += p.second.accel * 0.01f;
                        }

                        dpa.actor->setGlobalPose(pose);
                        t.position = worlds::px2glm(pose.p);
                        t.rotation = worlds::px2glm(pose.q);
                        //rd->setLinearVelocity(worlds::glm2px(linVel));
                    //}
                    }
                });

            } else {
                entt::entity lEnt = _this->playerLocospheres[pPos.id];
                auto& dpa = _this->reg->get<worlds::DynamicPhysicsActor>(lEnt);
                auto* rd = (physx::PxRigidDynamic*)dpa.actor;

                auto pose = dpa.actor->getGlobalPose();
                pose.p = worlds::glm2px(pPos.pos);
                pose.q = worlds::glm2px(pPos.rot);
                dpa.actor->setGlobalPose(pose);
                rd->setLinearVelocity(worlds::glm2px(pPos.linVel));
                rd->setAngularVelocity(worlds::glm2px(pPos.angVel));
            }
        }

        if (evt.packet->data[0] == MessageType::OtherPlayerJoin) {
            msgs::OtherPlayerJoin opj;
            opj.fromPacket(evt.packet);

            PlayerRig newRig = _this->lsphereSys->createPlayerRig(*_this->reg);
            auto& lpc = _this->reg->get<LocospherePlayerComponent>(newRig.locosphere);
            lpc.isLocal = false;
            _this->playerLocospheres[opj.id] = newRig.locosphere;

            auto meshId = worlds::g_assetDB.addOrGetExisting("sourcemodel/models/konnie/isa/detroit/connor.mdl");
            auto devMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
            auto& connorWO = _this->reg->emplace<worlds::WorldObject>(newRig.locosphere, devMatId, meshId);
            worlds::setupSourceMaterials(meshId, connorWO);
        }

        if (evt.packet->data[0] == MessageType::OtherPlayerLeave) {
            msgs::OtherPlayerLeave opl;
            opl.fromPacket(evt.packet);

            PlayerRig& rig = _this->reg->get<PlayerRig>(_this->playerLocospheres[opl.id]);

            _this->reg->destroy(rig.fender);
            _this->reg->destroy(rig.locosphere);
            rig.fenderJoint->release();
            _this->playerLocospheres[opl.id] = entt::null;
        }

        if (evt.packet->data[0] == MessageType::RigidbodySync) {
            msgs::RigidbodySync rSync;
            rSync.fromPacket(evt.packet);

            auto& dpa = _this->reg->get<worlds::DynamicPhysicsActor>((entt::entity)rSync.entId);
            auto* rd = (physx::PxRigidDynamic*)dpa.actor;

            auto pose = dpa.actor->getGlobalPose();
            pose.p = worlds::glm2px(rSync.pos);
            pose.q = worlds::glm2px(rSync.rot);
            dpa.actor->setGlobalPose(pose);
            rd->setLinearVelocity(worlds::glm2px(rSync.linVel));
            rd->setAngularVelocity(worlds::glm2px(rSync.angVel));
        }
    }

    void EventHandler::onPlayerJoin(NetPlayer& player, void* vp) {
        EventHandler* _this = (EventHandler*)vp;

        // setup the new player's locosphere!
        PlayerRig newRig = _this->lsphereSys->createPlayerRig(*_this->reg);
        auto& lpc = _this->reg->get<LocospherePlayerComponent>(newRig.locosphere);
        lpc.isLocal = false;
        auto& sp = _this->reg->emplace<ServerPlayer>(newRig.locosphere);
        sp.lastAcknowledgedInput = 0;
        _this->playerLocospheres[player.idx] = newRig.locosphere;

        msgs::OtherPlayerJoin opj;
        opj.id = player.idx;
        _this->server->broadcastExcluding(opj.toPacket(ENET_PACKET_FLAG_RELIABLE), player.idx);

        _this->reg->view<SyncedRB, worlds::DynamicPhysicsActor>().each([&](auto ent, worlds::DynamicPhysicsActor& dpa) {
            auto* rd = (physx::PxRigidDynamic*)dpa.actor;
            auto pose = dpa.actor->getGlobalPose();

            msgs::RigidbodySync rSync;
            rSync.entId = (uint32_t)ent;

            rSync.pos = worlds::px2glm(pose.p);
            rSync.rot = worlds::px2glm(pose.q);
            rSync.linVel = worlds::px2glm(rd->getLinearVelocity());
            rSync.angVel = worlds::px2glm(rd->getAngularVelocity());

            enet_peer_send(player.peer, NetChannel_World, rSync.toPacket(ENET_PACKET_FLAG_RELIABLE));
            });
    }

    void EventHandler::onPlayerLeave(NetPlayer& player, void* vp) {
        EventHandler* _this = (EventHandler*)vp;

        // destroy the full rig
        PlayerRig& rig = _this->reg->get<PlayerRig>(_this->playerLocospheres[player.idx]);

        _this->reg->destroy(rig.fender);
        _this->reg->destroy(rig.locosphere);
        rig.fenderJoint->release();
        _this->playerLocospheres[player.idx] = entt::null;

        msgs::OtherPlayerLeave opl;
        opl.id = player.idx;
        _this->server->broadcastExcluding(opl.toPacket(ENET_PACKET_FLAG_RELIABLE), player.idx);
    }
}
