#include "Client.hpp"
#include <Log.hpp>

namespace converge {
    Client::Client() {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
    }

    void Client::connect(ENetAddress address) {
        serverPeer = enet_host_connect(host, &address, 2, 0);
        ENetEvent event;

        if (enet_host_service (host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            logMsg("connection succeded!");
        } else {
            enet_peer_reset (serverPeer);
        }

    }
}
