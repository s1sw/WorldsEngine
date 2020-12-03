#pragma once
#include "Network.hpp"

namespace converge {
    class Client : public NetBase {
    public:
        Client();
        void connect(ENetAddress address);
        ENetPeer* serverPeer;
    private:
        void handleConnection(const ENetEvent& evt) override;
        void handleDisconnection(const ENetEvent& evt) override;

    };
}
