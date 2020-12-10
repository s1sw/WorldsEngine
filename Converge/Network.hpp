#pragma once
#include "NetMessage.hpp"
#include <limits>
#include <stdint.h>
#include <enet/enet.h>

namespace converge {
    enum DisconnectReason {
        // enet uses 0 to mean unknown, so we'll do that as well
        DisconnectReason_Unknown = 0,
        DisconnectReason_ServerFull,
        DisconnectReason_Kicked,
        DisconnectReason_ServerError,
        DisconnectReason_ClientError,
        DisconnectReason_ServerShutdown,
        DisconnectReason_PlayerLeaving
    };

    enum NetChannel {
        NetChannel_Default,
        NetChannel_Player,
        NetChannel_World,
        NetChannel_Count
    };

    const int MAX_PLAYERS = 32;

    struct NetPlayer {
        bool present;
        ENetPeer* peer;
        uint8_t idx;

        static_assert(MAX_PLAYERS < std::numeric_limits<decltype(idx)>::max());
    };

    typedef void(*MessageCallback)(const ENetEvent&, void*);
    typedef void(*ServerConnectCallback)(NetPlayer&, void*);
    typedef void(*ClientConnectCallback)(void*);

    class NetBase {
    public:
        NetBase();
        void processMessages(MessageCallback callback);
        void setCallbackCtx(void* obj) { callbackCtx = obj; }
        virtual ~NetBase() {}
    protected:
        virtual void handleReceivedPacket(const ENetEvent& evt, MessageCallback callback);
        virtual void handleConnection(const ENetEvent& evt) = 0;
        virtual void handleDisconnection(const ENetEvent& evt) = 0;
        // callbacks
        MessageCallback msgCallback;
        void* callbackCtx;
        ENetHost* host;
    };
}
