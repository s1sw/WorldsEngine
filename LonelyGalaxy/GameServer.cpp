#include "GameServer.hpp"
#include <entt/entity/registry.hpp>
#include "LocospherePlayerSystem.hpp"
#include <Core/Console.hpp>
#include "SyncedRB.hpp"

namespace lg {
    worlds::ConVar sendRate { "net_sendRate", "5", "Send rate in simulation ticks. 0 = 1 packet per tick" };

    GameServer::GameServer(entt::registry& reg)
        : reg(reg) {
        netServer = new Server;
        netServer->setCallbackCtx(this);
        netServer->setConnectionCallback(onPlayerJoin);
        netServer->setDisconnectionCallback(onPlayerLeave);
        netServer->start();
    }

    void GameServer::simulate(float simStep) {
        netServer->processMessages(onPacket);

        reg.view<ServerPlayer, LocospherePlayerComponent>().each([](auto, ServerPlayer& sp, LocospherePlayerComponent& lpc) {
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
                if (!netServer->players[i].present) continue;

                auto& sp = reg.get<ServerPlayer>(playerLocospheres[i]);
                auto& dpa = reg.get<worlds::DynamicPhysicsActor>(playerLocospheres[i]);
                auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                auto pose = dpa.actor->getGlobalPose();

                msgs::PlayerPosition pPos;
                pPos.id = i;
                pPos.pos = worlds::px2glm(pose.p);
                pPos.rot = worlds::px2glm(pose.q);
                pPos.linVel = worlds::px2glm(rd->getLinearVelocity());
                pPos.angVel = worlds::px2glm(rd->getAngularVelocity());
                pPos.inputIdx = sp.lastAcknowledgedInput;

                netServer->broadcastPacket(pPos.toPacket(0), NetChannel_Player);
            }

            reg.view<SyncedRB, worlds::DynamicPhysicsActor>().each([&](auto ent, worlds::DynamicPhysicsActor& dpa) {
                auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                auto pose = dpa.actor->getGlobalPose();

                if (rd->isSleeping()) return;

                msgs::RigidbodySync rSync;
                rSync.entId = (uint32_t)ent;

                rSync.pos = worlds::px2glm(pose.p);
                rSync.rot = worlds::px2glm(pose.q);
                rSync.linVel = worlds::px2glm(rd->getLinearVelocity());
                rSync.angVel = worlds::px2glm(rd->getAngularVelocity());

                netServer->broadcastPacket(rSync.toPacket(0), NetChannel_World);
            });
            syncTimer = 0;
        }
    }

    void GameServer::onSceneStart(entt::registry& reg) {
        //msgs::SetScene setScene;
        //setScene.sceneName = g_engine->getCurrentSceneInfo().name;
        //netServer->broadcastPacket(
                //setScene.toPacket(ENET_PACKET_FLAG_RELIABLE),
                //NetChannel_Default);

        // recreate player locospheres
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!netServer->players[i].present) return;
            PlayerRig newRig = LocospherePlayerSystem::createPlayerRig(reg);
            auto& lpc = reg.get<LocospherePlayerComponent>(newRig.locosphere);
            lpc.isLocal = false;
            auto& sp = reg.emplace<ServerPlayer>(newRig.locosphere);
            sp.lastAcknowledgedInput = 0;
            playerLocospheres[i] = newRig.locosphere;
        }
    }

    void GameServer::onPacket(const ENetEvent& evt, void* ctx) {
        auto* packet = evt.packet;
        GameServer* _this = (GameServer*)ctx;

        if (packet->data[0] == MessageType::PlayerInput) {
            msgs::PlayerInput pi;
            pi.fromPacket(packet);

            // send it to the proper locosphere!
            uint8_t idx = (uintptr_t)evt.peer->data;
            entt::entity locosphereEnt = _this->playerLocospheres[idx];

            auto& sp = _this->reg.get<ServerPlayer>(locosphereEnt);
            sp.inputMsgs.push_back(pi);
            sp.lastAcknowledgedInput = pi.inputIdx;
        }
    }

    void GameServer::onPlayerJoin(NetPlayer& player, void* ctx) {
        GameServer* _this = (GameServer*)ctx;

        // setup the new player's locosphere!
        PlayerRig newRig = LocospherePlayerSystem::createPlayerRig(_this->reg);
        auto& lpc = _this->reg.get<LocospherePlayerComponent>(newRig.locosphere);
        lpc.isLocal = false;
        auto& sp = _this->reg.emplace<ServerPlayer>(newRig.locosphere);
        sp.lastAcknowledgedInput = 0;
        _this->playerLocospheres[player.idx] = newRig.locosphere;

        msgs::OtherPlayerJoin opj;
        opj.id = player.idx;
        _this->netServer->broadcastExcluding(opj.toPacket(ENET_PACKET_FLAG_RELIABLE), player.idx);

        _this->reg.view<SyncedRB, worlds::DynamicPhysicsActor>().each([&](auto ent, worlds::DynamicPhysicsActor& dpa) {
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

    void GameServer::onPlayerLeave(NetPlayer& player, void* ctx) {
        GameServer* _this = (GameServer*)ctx;

        // destroy the full rig
        PlayerRig& rig = _this->reg.get<PlayerRig>(_this->playerLocospheres[player.idx]);

        _this->reg.destroy(rig.fender);
        _this->reg.destroy(rig.locosphere);
        rig.fenderJoint->release();
        _this->playerLocospheres[player.idx] = entt::null;

        msgs::OtherPlayerLeave opl;
        opl.id = player.idx;
        _this->netServer->broadcastExcluding(opl.toPacket(ENET_PACKET_FLAG_RELIABLE), player.idx);
    }
}
