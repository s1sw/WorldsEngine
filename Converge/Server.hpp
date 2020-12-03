#pragma once
#include <queue>
#include "NetMessage.hpp"
#include <array>
#include <enet/enet.h>
#include "Network.hpp"

namespace converge {
    class Server : public NetBase {
    public:
        Server();
        ~Server();
        void start();
        void stop();
        void setConnectionCallback(ServerConnectCallback callback) { connectCallback = callback; }
        void setDisconnectionCallback(ServerConnectCallback callback) { disconnectCallback = callback; }
        NetPlayer players[MAX_PLAYERS];
    protected:
        bool findFreePlayerSlot(uint8_t& slot);
        void handleConnection(const ENetEvent& evt) override;
        void handleDisconnection(const ENetEvent& evt) override;
        ServerConnectCallback connectCallback;
        ServerConnectCallback disconnectCallback;
    };
}
