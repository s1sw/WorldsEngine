#pragma once
#include "NetMessage.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "GameServer.hpp"
#include "GameClient.hpp"
#include <entt/entity/fwd.hpp>

namespace lg {
    class MultiplayerManager {
    public:
        MultiplayerManager(entt::registry& reg, bool isDedicated);
        void simulate(float simStep);
        void update(float deltaTime);
        void onSceneStart(entt::registry& reg);
    private:
        entt::registry& reg;
        bool isServer = false;
        void simulateServer(float simStep);
        void simulateClient(float simStep);
        GameServer* gameServer;
        GameClient* gameClient;
    };
}
