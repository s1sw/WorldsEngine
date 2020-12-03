#include "Server.hpp"
#include <Log.hpp>

namespace converge {
    NetBase::NetBase() : host(nullptr) {
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
                    handleReceivedPacket(evt, callback);
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    void NetBase::handleReceivedPacket(const ENetEvent& evt, MessageCallback callback) {
        NetworkMessage msg;
        // Advance by one byte to account for ID byte
        msg.data = evt.packet->data + 1;
        msg.dataLen = evt.packet->dataLength - 1;

        if (evt.packet->dataLength == 0) {
            logErr("Zero-length network message????");
            return;
        }

        msg.msgType = evt.packet->data[0];

        if (callback)
            callback(msg, callbackCtx);
    }
}
