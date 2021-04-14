#include "MultiplayerManager.hpp"
#include <entt/entity/registry.hpp>
#include <ImGui/imgui.h>

namespace lg {
    MultiplayerManager::MultiplayerManager(entt::registry& reg, bool isDedicated) 
        : reg(reg)
        , isServer(isDedicated) {

        if (isServer) {
            gameServer = new GameServer{reg};
        } else {
            gameClient = new GameClient{reg};
        }
    }

    void MultiplayerManager::simulate(float simStep) {
        if (isServer)
            simulateServer(simStep);
        else
            simulateClient(simStep);
    }

    void MultiplayerManager::update(float deltaTime) {
        if (!isServer)
            gameClient->update(deltaTime);
    }

    void MultiplayerManager::onSceneStart(entt::registry& reg) {
        if (isServer)
            gameServer->onSceneStart(reg);
    }

    void MultiplayerManager::simulateServer(float simStep) {
        gameServer->simulate(simStep);
    }

    void MultiplayerManager::simulateClient(float simStep) {
        gameClient->simulate(simStep);
    }
}
