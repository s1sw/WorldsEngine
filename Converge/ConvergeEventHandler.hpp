#pragma once
#include <enet/enet.h>
#include <IGameEventHandler.hpp>
#include <Console.hpp>
#include "Engine.hpp"
#include "NetMessage.hpp"
#include "PidController.hpp"
#include <Camera.hpp>
#include "LocospherePlayerSystem.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <deque>

namespace converge {
    struct ServerPlayer {
        uint16_t lastAcknowledgedInput;
        std::deque<msgs::PlayerInput> inputMsgs;
    };

    class EventHandler : public worlds::IGameEventHandler {
    public:
        EventHandler(bool dedicatedServer);
        void init(entt::registry& registry, worlds::EngineInterfaces interfaces) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void simulate(entt::registry& registry, float simStep) override;
        void onSceneStart(entt::registry& reg) override;
        void shutdown(entt::registry& registry) override;
    private:
        static void onServerPacket(const ENetEvent&, void*);
        static void onClientPacket(const ENetEvent&, void*);
        static void onPlayerJoin(NetPlayer&, void*);
        static void onPlayerLeave(NetPlayer&, void*);
        worlds::IVRInterface* vrInterface;
        worlds::VKRenderer* renderer;
        worlds::InputManager* inputManager;
        worlds::Camera* camera;
        worlds::WorldsEngine* engine;
        LocospherePlayerSystem* lsphereSys;
        entt::registry* reg;
        bool isDedicated;
        entt::entity otherLocosphere;
        Client* client;
        Server* server;
        entt::entity lHandEnt, rHandEnt;
        physx::PxD6Joint* lHandJoint, *rHandJoint;
        entt::entity playerLocospheres[MAX_PLAYERS];
        bool setClientInfo = false;
        uint16_t clientInputIdx = 0;

        struct LocosphereState {
            glm::vec3 pos;
            glm::vec3 linVel;
            glm::vec3 angVel;
            glm::vec3 accel;
            uint16_t inputIndex;
        };

        msgs::PlayerInput lastSent;

        std::unordered_map<uint16_t, LocosphereState> pastLocosphereStates;
        float lsphereErr[128];
        uint32_t lsphereErrIdx = 0;
    };
}
