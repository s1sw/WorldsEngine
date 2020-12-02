#include "Server.hpp"
#include <Fatal.hpp>

namespace converge {
    Server::Server() {
    }

    void Server::start() {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = 3011;
        host = enet_host_create(&address, 32, 2, 0, 0);

        if (host == NULL) {
            fatalErr("An error occurred while trying to create an ENet server host.");
        }
    }

    void Server::stop() {
        enet_host_destroy(host);
    }
}
