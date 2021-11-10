#pragma once
#include "Client.hpp"
#include "NetMessage.hpp"
#include <entt/entity/fwd.hpp>
#include <robin_hood.h>

namespace lg {
    class GameClient {
    public:
        GameClient(entt::registry& reg);
        void simulate(float simStep);
        void update(float deltaTime);
    private:
        void handleLocalPosUpdate(msgs::PlayerPosition& pPos);
        static void onPacket(const ENetEvent&, void*);
        Client* netClient;
        entt::registry& reg;
        entt::entity playerLocospheres[MAX_PLAYERS];

        struct LocosphereState {
            glm::vec3 pos;
            glm::vec3 linVel;
            glm::vec3 angVel;
            glm::vec3 accel;
            uint16_t inputIndex;
        };

        robin_hood::unordered_map<uint16_t, LocosphereState> pastLocosphereStates;
        float lsphereErr[128];
        uint32_t lsphereErrIdx = 0;
        uint16_t clientInputIdx = 0;
        msgs::PlayerInput lastSent;
    };
}
