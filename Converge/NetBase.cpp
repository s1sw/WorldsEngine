#include "Server.hpp"
#include <Log.hpp>

namespace converge {
    NetBase::NetBase() : host(nullptr) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            players[i].present = false;
            players[i].idx = i;
        }
    }

    void NetBase::processMessages(MessageCallback callback) {
        ENetEvent evt;
        while (enet_host_service(host, &evt, 0) > 0) {
            switch (evt.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    handleConnection(evt);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    handleDisconnection(evt);
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    handleReceivedPacket(evt);
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    bool NetBase::findFreePlayerSlot(uint8_t& slot) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!players[i].present) {
                slot = i;
                return true;
            }
        }

        // no free slots, server is full
        return false;
    }

    void NetBase::handleConnection(const ENetEvent& evt) {
        logMsg("received new connection");
        // allocate new player
        uint8_t newIdx;

        if (!findFreePlayerSlot(newIdx)) {
            logWarn("rejecting connection as server is full :(");
            enet_peer_disconnect(evt.peer, DisconnectReason_ServerFull);
            return;
        }

        // start setting up stuff for the player
        NetPlayer& np = players[newIdx]; 
        np.peer = evt.peer;
        np.present = true;

        connectCallback(np, callbackCtx);
    }

    void NetBase::handleDisconnection(const ENetEvent& evt) {
    }
}
