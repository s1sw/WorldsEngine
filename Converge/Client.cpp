#include "Client.hpp"
#include <Log.hpp>

namespace converge {
    Client::Client() {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
    }

    void Client::connect(ENetAddress address) {
        serverPeer = enet_host_connect(host, &address, 2, 0);
        ENetEvent event;

        if (enet_host_service (host, &event, 100) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            logMsg("connection succeded!");
        } else {
            logErr("connection failed :(");
            enet_peer_reset (serverPeer);
        }
    }

    void Client::handleConnection(const ENetEvent& evt) {
        logMsg("connected! ping is %u", serverPeer->roundTripTime);
    }
    
    void Client::handleDisconnection(const ENetEvent& evt) {
        logMsg("disconnected :(");
    }
}
