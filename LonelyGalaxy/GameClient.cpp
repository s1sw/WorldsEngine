#include "GameClient.hpp"
#include <Core/AssetDB.hpp>
#include <Core/Console.hpp>
#include "LocospherePlayerSystem.hpp"
#include "NetMessage.hpp"
#include <entt/entity/registry.hpp>
#include <Physics/PhysicsActor.hpp>
#include <ImGui/imgui.h>

namespace lg {
    const uint16_t CONVERGE_PORT = 3011;

    GameClient::GameClient(entt::registry& reg)
        : reg(reg) {
        netClient = new Client;
        netClient->setCallbackCtx(this);

        worlds::g_console->registerCommand([&](void*, const char*) {
            if (!netClient->serverPeer ||
                    netClient->serverPeer->state != ENET_PEER_STATE_CONNECTED) {
                logErr("not connected!");
                return;
            }

            netClient->disconnect();
        }, "disconnect", "Disconnect from the server.", nullptr);

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            if (netClient->isConnected()) {
                logErr("already connected! disconnect first.");
            }

            // assume the argument is an address
            ENetAddress addr;
            enet_address_set_host(&addr, arg);
            addr.port = CONVERGE_PORT;

            netClient->connect(addr);
        }, "connect", "Connects to the specified server.", nullptr);
    }

    void GameClient::simulate(float simStep) {
        if (!netClient->isConnected()) return;
        entt::entity localLocosphereEnt = entt::null;
        LocospherePlayerComponent* localLpc;

        reg.view<LocospherePlayerComponent>().each([&](auto ent, LocospherePlayerComponent& lpc) {
            if (!lpc.isLocal) return;
            localLocosphereEnt = ent;
            localLpc = &lpc;
        });

        if (!reg.valid(localLocosphereEnt)) return;

        auto& dpa = reg.get<worlds::DynamicPhysicsActor>(localLocosphereEnt);
        if (ImGui::Begin("client dbg")) {
            ImGui::Text("curr input idx: %u", clientInputIdx);
            ImGui::Text("past locosphere state count: %zu", pastLocosphereStates.size());
        }
        ImGui::End();

        // send input to server
        msgs::PlayerInput pi {
            .xzMoveInput = localLpc->xzMoveInput,
            .sprint = localLpc->sprint,
            .jump = localLpc->jump,
            .inputIdx = clientInputIdx
        };
        netClient->sendPacketToServer(pi.toPacket(0), NetChannel_Player);

        auto pose = dpa.actor->getGlobalPose();
        auto* rd = (physx::PxRigidDynamic*)dpa.actor;

        static glm::vec3 lastVel{ 0.0f };

        pastLocosphereStates.insert({
            clientInputIdx,
            LocosphereState {
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

    void GameClient::update(float deltaTime) {
        netClient->processMessages(onPacket);

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

        if (netClient->isConnected()) {
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

    void GameClient::handleLocalPosUpdate(msgs::PlayerPosition& pPos) {
        reg.view<LocospherePlayerComponent, worlds::DynamicPhysicsActor, Transform>()
            .each([&](auto, LocospherePlayerComponent& lpc, worlds::DynamicPhysicsActor& dpa, Transform& t) {
            if (lpc.isLocal) {
                auto pose = dpa.actor->getGlobalPose();
                auto pastStateIt = pastLocosphereStates.find(pPos.inputIdx);

                if (pastStateIt != pastLocosphereStates.end()) {
                    auto pastState = pastLocosphereStates.at(pPos.inputIdx);
                    float err = glm::length(pastState.pos - pPos.pos);

                    lsphereErr[lsphereErrIdx] = err;
                    lsphereErrIdx++;

                    if (lsphereErrIdx == 128)
                        lsphereErrIdx = 0;
                }

                auto it = pastLocosphereStates.begin();
                while (it != pastLocosphereStates.end()) {
                    if (it->first <= pPos.inputIdx)
                        it = pastLocosphereStates.erase(it);
                }

                pose.p = worlds::glm2px(pPos.pos);
                pose.q = worlds::glm2px(pPos.rot);
                //auto* rd = (physx::PxRigidDynamic*)dpa.actor;
                glm::vec3 linVel = pPos.linVel;

                for (auto& p : pastLocosphereStates) {
                    pose.p += worlds::glm2px(linVel * 0.01f);
                    linVel += p.second.accel * 0.01f;
                }

                dpa.actor->setGlobalPose(pose);
                t.position = worlds::px2glm(pose.p);
                t.rotation = worlds::px2glm(pose.q);
                //rd->setLinearVelocity(worlds::glm2px(linVel));
            }
        });
    }

    void GameClient::onPacket(const ENetEvent& evt, void* vp) {
        GameClient* _this = (GameClient*)vp;

        if (evt.packet->data[0] == MessageType::PlayerPosition) {
            msgs::PlayerPosition pPos;
            pPos.fromPacket(evt.packet);

            if (pPos.id == _this->netClient->serverSideID) {
                _this->handleLocalPosUpdate(pPos);
            } else {
                entt::entity lEnt = _this->playerLocospheres[pPos.id];
                auto& dpa = _this->reg.get<worlds::DynamicPhysicsActor>(lEnt);
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

            PlayerRig newRig = LocospherePlayerSystem::createPlayerRig(_this->reg);
            auto& lpc = _this->reg.get<LocospherePlayerComponent>(newRig.locosphere);
            lpc.isLocal = false;
            _this->playerLocospheres[opj.id] = newRig.locosphere;
        }

        if (evt.packet->data[0] == MessageType::OtherPlayerLeave) {
            msgs::OtherPlayerLeave opl;
            opl.fromPacket(evt.packet);

            PlayerRig& rig = _this->reg.get<PlayerRig>(_this->playerLocospheres[opl.id]);

            _this->reg.destroy(rig.fender);
            _this->reg.destroy(rig.locosphere);
            rig.fenderJoint->release();
            _this->playerLocospheres[opl.id] = entt::null;
        }

        if (evt.packet->data[0] == MessageType::RigidbodySync) {
            msgs::RigidbodySync rSync;
            rSync.fromPacket(evt.packet);

            auto& dpa = _this->reg.get<worlds::DynamicPhysicsActor>((entt::entity)rSync.entId);
            auto* rd = (physx::PxRigidDynamic*)dpa.actor;

            auto pose = dpa.actor->getGlobalPose();
            pose.p = worlds::glm2px(rSync.pos);
            pose.q = worlds::glm2px(rSync.rot);
            dpa.actor->setGlobalPose(pose);
            rd->setLinearVelocity(worlds::glm2px(rSync.linVel));
            rd->setAngularVelocity(worlds::glm2px(rSync.angVel));
        }
    }
}
