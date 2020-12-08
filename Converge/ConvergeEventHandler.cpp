#include "ConvergeEventHandler.hpp"
#include <RichPresence.hpp>
#include <openvr.h>
#include <Log.hpp>
#include <Render.hpp>
#include "AssetDB.hpp"
#include "DebugArrow.hpp"
#include "NetMessage.hpp"
#include "PhysicsActor.hpp"
#include <physx/PxRigidDynamic.h>
#include "SourceModelLoader.hpp"
#include "Transform.hpp"
#include <OpenVRInterface.hpp>
#include <Physics.hpp>
#include <Console.hpp>
#include <imgui.h>
#include <MatUtil.hpp>
#include <Engine.hpp>
#include "NameComponent.hpp"
#include "LocospherePlayerSystem.hpp"
#include "PhysHandSystem.hpp"
#include <enet/enet.h>
#include <JobSystem.hpp>
#include "CreateModelObject.hpp"
#include "ObjectParentSystem.hpp"
#include <core.h>

namespace converge {
    const uint16_t CONVERGE_PORT = 3011;

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
        }

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            if (isDedicated) {
                logErr("this is a server! what are you trying to do???");
                return;
            }
            
            // assume the argument is an address
            ENetAddress addr;
            enet_address_set_host(&addr, arg);
            addr.port = CONVERGE_PORT;

            client->connect(addr);
            }, "connect", "Connects to the specified server.", nullptr);

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
    }

    void EventHandler::update(entt::registry& registry, float, float) {
        if (client) {
            client->processMessages(onClientPacket);

#ifdef DISCORD_RPC
            if (!setClientInfo) {
                discord::User currUser;
                auto res = worlds::discordCore->UserManager().GetCurrentUser(&currUser);

                if (res == discord::Result::Ok) {
                    logMsg("got user info, setting client info for %s#%s with id %lu...", currUser.GetUsername(), currUser.GetDiscriminator(), currUser.GetId());
                    client->setClientInfo(1, currUser.GetId(), 1);
                    setClientInfo = true;
                }
            }
#endif
        }

        g_dbgArrows->newFrame();
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

        auto& t = registry.get<Transform>(localLocosphereEnt);
        auto& t2 = registry.get<Transform>(otherLocosphere);
        auto& lpc2 = registry.get<LocospherePlayerComponent>(otherLocosphere);
        glm::vec3 dir = t.position - t2.position;
        float sqDist = glm::length2(dir);
        dir.y = 0.0f;
        dir = glm::normalize(dir);

        if (sqDist > 16.0f)
            lpc2.xzMoveInput = glm::vec2 { dir.x, dir.z };
        else
            lpc2.xzMoveInput = glm::vec2 { 0.0f };
        lpc2.sprint = sqDist > 100.0f; 
    }

    void EventHandler::simulate(entt::registry& registry, float) {
        if (isDedicated) {
            server->processMessages(onServerPacket);

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
                
                server->broadcastPacket(pPos.toPacket(0));
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
            client->sendPacketToServer(pi.toPacket(0));

            auto pose = dpa.actor->getGlobalPose();

            pastLocosphereStates.insert({ clientInputIdx, { worlds::px2glm(pose.p), clientInputIdx }});
            clientInputIdx++;
            lastSent = pi;
        }
    }

    void EventHandler::onSceneStart(entt::registry& registry) {
        // create... a SECOND LOCOSPHERE :O
        PlayerRig other = lsphereSys->createPlayerRig(registry);
        auto& lpc = registry.get<LocospherePlayerComponent>(other.locosphere);
        lpc.isLocal = false;
        lpc.sprint = false;
        lpc.maxSpeed = 0.0f;
        lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);
        otherLocosphere = other.locosphere;

        auto meshId = worlds::g_assetDB.addOrGetExisting("sourcemodel/models/konnie/isa/detroit/connor.mdl");
        auto devMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
        auto& connorWO = registry.emplace<worlds::WorldObject>(other.locosphere, devMatId, meshId);
        worlds::setupSourceMaterials(meshId, connorWO);

        // create our lil' pal the player
        if (!isDedicated) {
            PlayerRig other = lsphereSys->createPlayerRig(registry);
            auto& lpc = registry.get<LocospherePlayerComponent>(other.locosphere);
            lpc.isLocal = true;
            lpc.sprint = false;
            lpc.maxSpeed = 0.0f;
            lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);

            if (vrInterface) {
                auto matId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
                auto saberId = worlds::g_assetDB.addOrGetExisting("saber.wmdl");

                lHandEnt = registry.create();
                auto& lhWO = registry.emplace<worlds::WorldObject>(lHandEnt, matId, saberId);
                lhWO.materials[0] = worlds::g_assetDB.addOrGetExisting("Materials/saber_blade.json");
                lhWO.materials[1] = matId;
                lhWO.presentMaterials[1] = true;
                auto& lht = registry.emplace<Transform>(lHandEnt);
                lht.position = glm::vec3(0.5f, 1.0f, 0.0f);
                registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

                rHandEnt = registry.create();
                auto& rhWO = registry.emplace<worlds::WorldObject>(rHandEnt, matId, saberId);
                rhWO.materials[0] = worlds::g_assetDB.addOrGetExisting("Materials/saber_blade.json");
                rhWO.materials[1] = matId;
                rhWO.presentMaterials[1] = true;
                auto& rht = registry.emplace<Transform>(rHandEnt);
                rht.position = glm::vec3(-0.5f, 1.0f, 0.0f);
                registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

                auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                // Using the reference returned by this doesn't work unfortunately.
                registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

                auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
                auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

                rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));
                lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));

                worlds::updatePhysicsShapes(rwActor);
                worlds::updatePhysicsShapes(lwActor);

                worlds::g_scene->addActor(*rActor);
                worlds::g_scene->addActor(*lActor);

                physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
                physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

                auto& lHandPhys = registry.emplace<PhysHand>(lHandEnt);
                auto& rHandPhys = registry.emplace<PhysHand>(rHandEnt);

                lHandPhys.locosphere = other.locosphere;
                rHandPhys.locosphere = other.locosphere;

                PIDSettings posSettings {1370.0f, 0.0f, 100.0f};
                PIDSettings rotSettings {2.5f, 0.0f, 0.2f};

                lHandPhys.posController.acceptSettings(posSettings);
                lHandPhys.rotController.acceptSettings(rotSettings);

                rHandPhys.posController.acceptSettings(posSettings);
                rHandPhys.rotController.acceptSettings(rotSettings);

                lHandPhys.follow = FollowHand::LeftHand;
                rHandPhys.follow = FollowHand::RightHand;

                auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(other.fender).actor;

                lHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, lActor, physx::PxTransform { physx::PxIdentity });
                lHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.25f});
                lHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

                rHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, rActor, physx::PxTransform { physx::PxIdentity });
                rHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.25f});
                rHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
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
            auto& lpc = _this->reg->get<LocospherePlayerComponent>(locosphereEnt);
            lpc.xzMoveInput = pi.xzMoveInput;
            lpc.sprint = pi.sprint; 
            lpc.jump |= pi.jump;

            auto& sp = _this->reg->get<ServerPlayer>(locosphereEnt);
            sp.lastAcknowledgedInput = pi.inputIdx;
        }
    }

    void EventHandler::onClientPacket(const ENetEvent& evt, void* vp) {
        EventHandler* _this = (EventHandler*)vp;

        if (evt.packet->data[0] == MessageType::PlayerPosition) {
            msgs::PlayerPosition pPos;
            pPos.fromPacket(evt.packet);

            if (pPos.id == _this->client->serverSideID) {
                _this->reg->view<LocospherePlayerComponent, worlds::DynamicPhysicsActor>()
                    .each([&](auto, LocospherePlayerComponent& lpc, worlds::DynamicPhysicsActor& dpa) {
                    if (lpc.isLocal) {
                        auto pose = dpa.actor->getGlobalPose();
                        auto pastStateIt = _this->pastLocosphereStates.find(pPos.inputIdx);

                        if (pastStateIt != _this->pastLocosphereStates.end()) {
                            auto pastState = _this->pastLocosphereStates.at(pPos.inputIdx);
                            
                            if (glm::distance(pastState.pos, pPos.pos) > 0.1f) {
                                pose.p = worlds::glm2px(pPos.pos);
                                pose.q = worlds::glm2px(pPos.rot);
                                dpa.actor->setGlobalPose(pose);
                                auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                                rd->setLinearVelocity(worlds::glm2px(pPos.linVel));
                                rd->setAngularVelocity(worlds::glm2px(pPos.angVel));
                            }
                        } else {
                            pose.p = worlds::glm2px(pPos.pos);
                            pose.q = worlds::glm2px(pPos.rot);
                            dpa.actor->setGlobalPose(pose);
                            auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                            rd->setLinearVelocity(worlds::glm2px(pPos.linVel));
                            rd->setAngularVelocity(worlds::glm2px(pPos.angVel));
                        }
                    }
                });

                _this->pastLocosphereStates.erase(std::erase_if(_this->pastLocosphereStates, [&](auto& k) {
                    return k.first <= pPos.inputIdx;
                }));
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
