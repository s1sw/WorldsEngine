#include "Server.hpp"
#include <Fatal.hpp>
#include <Log.hpp>
#include "Console.hpp"

namespace converge {
    Server::Server() 
        : connectCallback(nullptr)
        , disconnectCallback(nullptr) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            players[i].present = false;
            players[i].idx = i;
            players[i].peer = nullptr;
        }

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            int id = atoi(arg);
            enet_peer_disconnect(players[id].peer, DisconnectReason_ServerShutdown);
        }, "server_kick", "Kicks a player.", nullptr);
    }

    Server::~Server() {
        stop();
    }

    void Server::start() {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = 3011;
        host = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);

        if (host == NULL) {
            fatalErr("An error occurred while trying to create an ENet server host.");
        }

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].peer) {
                    logMsg("player %i: present %i, %u RTT", i, players[i].present, players[i].peer->roundTripTime);
                } else {
                    logMsg("player %i: present %i", i, players[i].present);
                }
            }

        }, "server_printRTT", "", nullptr);
    }

    void Server::handleConnection(const ENetEvent& evt) {
        logMsg("received new connection");
        // allocate new player
        uint8_t newIdx;

        if (!findFreePlayerSlot(newIdx)) {
            logWarn("rejecting connection as server is full :(");
            enet_peer_disconnect(evt.peer, DisconnectReason_ServerFull);
            return;
        }

        logMsg("new player has idx of %i", newIdx);

        // start setting up stuff for the player
        players[newIdx].peer = evt.peer;
        players[newIdx].present = true;

        if (connectCallback)
            connectCallback(players[newIdx], callbackCtx);

        evt.peer->data = (void*)(uintptr_t)newIdx;
    }

    bool Server::findFreePlayerSlot(uint8_t& slot) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!players[i].present) {
                slot = i;
                return true;
            }
        }

        // no free slots, server is full
        return false;
    }

    void Server::handleDisconnection(const ENetEvent& evt) {
        logMsg("received disconnection");
        uint8_t idx = (uint8_t)(uintptr_t)evt.peer->data;

        if (!players[idx].present) {
            return;
        }

        if (disconnectCallback)
            disconnectCallback(players[idx], callbackCtx);
        players[idx].present = false;
    }

    void Server::stop() {
        logMsg("server stopping");
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].present) {
                enet_peer_disconnect(players[i].peer, DisconnectReason_ServerShutdown);
            }
        }

        processMessages(nullptr);
        enet_host_destroy(host);
    }
}
