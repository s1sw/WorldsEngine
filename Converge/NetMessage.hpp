#pragma once
#include <cassert>
#include <string.h>
#include <stdint.h>
#include <enet/enet.h>

namespace converge {
    typedef enum {
        JoinRequest,
        JoinAccept
    } MessageType;

    namespace msgs {
#pragma pack (push, 1)
        template <typename T>
        struct SimpleDataPacket {
            ENetPacket* toPacket(uint32_t flags) const {
                return enet_packet_create(this, sizeof(T), flags);
            }

            void fromPacket(ENetPacket* packet) {
                assert(packet->dataLength == sizeof(T));
                memcpy(this, packet->data, sizeof(T));
            }
        };

        struct PlayerJoinRequest : public SimpleDataPacket<PlayerJoinRequest> {
            MessageType type = MessageType::JoinRequest;
            uint64_t gameVersion;
            uint64_t userAuthId;
            // this'll probably always be 0 (our auth system),
            // but it's nice to have in case we add other auth methods.
            uint16_t userAuthUniverse;
        };

        struct PlayerJoinAcceptance : public SimpleDataPacket<PlayerJoinRequest> {
            MessageType type = MessageType::JoinAccept;
            uint16_t serverSideID;            
        };
#pragma pack (pop)
    }
}
