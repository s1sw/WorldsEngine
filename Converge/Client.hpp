#pragma once
#include "Network.hpp"

namespace converge {
    class Client : public NetBase {
    public:
        Client();
        void connect(ENetAddress address);
        ENetPeer* serverPeer;
        uint16_t serverSideID;
        void sendPacketToServer(ENetPacket* p);
    private:
        void handleConnection(const ENetEvent& evt) override;
        void handleDisconnection(const ENetEvent& evt) override;

    };
}
