#pragma once
#include <array>
#include <enet/enet.h>

namespace converge {
    struct RemotePlayer {
        ENetPeer* peer;
    };

    template <int MaxPlayers>
    class Server {
    public:
        Server();
        void start();
        void stop();
        ENetHost* host;
        std::array<RemotePlayer, MaxPlayers> players;
    };
}
