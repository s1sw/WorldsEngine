#pragma once
#include "Server.hpp"
#include <entt/entity/fwd.hpp>
#include <deque>

namespace lg {
    struct ServerPlayer {
        uint16_t lastAcknowledgedInput;
        std::deque<msgs::PlayerInput> inputMsgs;
    };

    // Handles game-specific server stuff.
    class GameServer {
    public:
        GameServer(entt::registry& reg);
        void simulate(float simStep);
        void onSceneStart(entt::registry& registry);
    private:
        static void onPacket(const ENetEvent&, void*);
        static void onPlayerJoin(NetPlayer&, void*);
        static void onPlayerLeave(NetPlayer&, void*);
        int syncTimer = 0;
        Server* netServer;
        entt::registry& reg;
        entt::entity playerLocospheres[MAX_PLAYERS];
    };
}
