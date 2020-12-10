#pragma once
#include "Network.hpp"
#include <enet/enet.h>

namespace converge {
    class Client : public NetBase {
    public:
        Client();
        void connect(ENetAddress address);
        ENetPeer* serverPeer;
        uint16_t serverSideID;
        void sendPacketToServer(ENetPacket* p, NetChannel channel = NetChannel_Default);
        void setClientInfo(uint64_t gameVersion, uint64_t userAuthId, uint16_t userAuthUniverse);
        bool isConnected() { 
            return serverPeer && serverPeer->state == ENET_PEER_STATE_CONNECTED; 
        }

        void disconnect();
        ~Client();
    private:
        uint64_t gameVersion;
        uint64_t userAuthId;
        uint16_t userAuthUniverse;
        void handleConnection(const ENetEvent& evt) override;
        void handleDisconnection(const ENetEvent& evt) override;
        void handleReceivedPacket(const ENetEvent& evt, MessageCallback callback) override;
    };
}
