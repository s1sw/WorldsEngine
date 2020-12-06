#include "Client.hpp"
#include <Log.hpp>
#include "NetMessage.hpp"

namespace converge {
    Client::Client() {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
    }

    void Client::connect(ENetAddress address) {
        serverPeer = enet_host_connect(host, &address, 2, 0);
    }

    void Client::sendPacketToServer(ENetPacket* p) {
        int result = enet_peer_send(serverPeer, 0, p);

        if (result != 0) {
            logWarn("failed to send packet");
        }
        logMsg("sent packet");
    }

    void Client::handleConnection(const ENetEvent& evt) {
        logMsg("connected! ping is %u", serverPeer->roundTripTime);

        msgs::PlayerJoinRequest pjr;
        pjr.gameVersion = 1;
        pjr.userAuthId = 1;
        pjr.userAuthUniverse = 1;

        auto pjrPacket = pjr.toPacket(ENET_PACKET_FLAG_RELIABLE);

        sendPacketToServer(pjrPacket);
    }
    
    void Client::handleDisconnection(const ENetEvent& evt) {
        logMsg("disconnected :(  reason was %u", evt.data);
    }
}
