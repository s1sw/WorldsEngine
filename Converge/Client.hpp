#pragma once
#include <enet/enet.h>

namespace converge {
    class Client {
    public:
        Client();
        void connect(ENetAddress address);
        ENetHost* host;
        ENetPeer* serverPeer;
    };
}
