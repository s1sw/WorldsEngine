#pragma once
#include <queue>
#include "NetMessage.hpp"
#include <array>
#include <enet/enet.h>

namespace converge {
    enum DisconnectReason {
        // enet uses 0 to mean unknown, so we'll do that as well
        DisconnectReason_Unknown = 0,
        DisconnectReason_ServerFull,
        DisconnectReason_Kicked,
        DisconnectReason_ServerError,
        DisconnectReason_ClientError
    };

    const int MAX_PLAYERS = 32;

    struct NetPlayer {
        bool present;
        ENetPeer* peer;
        uint8_t idx;

        static_assert(MAX_PLAYERS < std::numeric_limits<decltype(idx)>::max());
    };

    typedef void(*MessageCallback)(const NetworkMessage&, void*);
    typedef void(*ConnectCallback)(NetPlayer&, void*);

    class NetBase {
    public:
        NetBase();
        std::array<NetPlayer, MAX_PLAYERS> players;
        void processMessages(MessageCallback callback);
        void setConnectionCallback(ConnectCallback callback) { connectCallback = callback; }
        void setDisconnectionCallback(ConnectCallback callback) { disconnectCallback = callback; }
        void setCallbackCtx(void* obj) { callbackCtx = obj; }
    protected:
        bool findFreePlayerSlot(uint8_t& slot);
        void handleConnection(const ENetEvent& evt);
        void handleDisconnection(const ENetEvent& evt);
        void handleReceivedPacket(const ENetEvent& evt);
        // callbacks
        MessageCallback msgCallback;
        ConnectCallback connectCallback;
        ConnectCallback disconnectCallback;
        void* callbackCtx;
        ENetHost* host;
    };

    class Server : public NetBase {
    public:
        Server();
        void start();
        void stop();
    };
}
