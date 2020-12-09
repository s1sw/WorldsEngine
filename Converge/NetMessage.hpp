#pragma once
#include "Log.hpp"
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string.h>
#include <stdint.h>
#include <enet/enet.h>
#include <string>

namespace converge {
    typedef enum {
        JoinRequest,
        JoinAccept,
        PlayerInput,
        PlayerPosition,
        OtherPlayerJoin,
        OtherPlayerLeave,
        SetScene,
        RigidbodySync
    } MessageType;

    namespace msgs {
#pragma pack (push, 1)
#define DATAPACKET(name) struct name : public SimpleDataPacket<name>
        template <typename T>
        struct SimpleDataPacket {
            ENetPacket* toPacket(uint32_t flags) const {
                return enet_packet_create(this, sizeof(T), flags);
            }

            void fromPacket(ENetPacket* packet) {
                assert(packet->dataLength == sizeof(T) && "Packet length mismatch!");
                assert(((T*)this)->type == packet->data[0] && "Packet type mismatch!");
                memcpy(this, packet->data, sizeof(T));
            }
        };

        DATAPACKET(PlayerJoinRequest) {
            MessageType type = MessageType::JoinRequest;
            uint64_t gameVersion;
            uint64_t userAuthId;
            // this'll probably always be 0 (our auth system),
            // but it's nice to have in case we add other auth methods.
            uint16_t userAuthUniverse;
        };

        DATAPACKET(PlayerJoinAcceptance) {
            MessageType type = MessageType::JoinAccept;
            uint16_t serverSideID;            
        };

        DATAPACKET(PlayerPosition) {
            MessageType type = MessageType::PlayerPosition;
            uint8_t id;
            glm::vec3 pos;
            glm::quat rot;
            glm::vec3 linVel;
            glm::vec3 angVel;
            uint16_t inputIdx;
        };

        DATAPACKET(PlayerInput) {
            MessageType type = MessageType::PlayerInput;
            glm::vec2 xzMoveInput;
            bool sprint;
            bool jump;
            uint16_t inputIdx;
        };

        DATAPACKET(OtherPlayerJoin) {
            MessageType type = MessageType::OtherPlayerJoin;
            uint8_t id;
        };

        DATAPACKET(OtherPlayerLeave) {
            MessageType type = MessageType::OtherPlayerLeave;
            uint8_t id;
        };

        DATAPACKET(RigidbodySync) {
            MessageType type = MessageType::RigidbodySync;
            uint32_t entId;
            glm::vec3 pos;
            glm::quat rot;
            glm::vec3 linVel;
            glm::vec3 angVel;
        };

        struct SetScene {
            MessageType type = MessageType::SetScene;
            std::string sceneName;

            ENetPacket* toPacket(uint32_t flags) {
                auto* packet = enet_packet_create(nullptr, sceneName.size() + sizeof(type) + sizeof(uint16_t), flags);
                
                packet->data[0] = type;
                uint16_t* nameLen = (uint16_t*)&packet->data[1];
                *nameLen = sceneName.size();

                memcpy(&packet->data[3], sceneName.data(), sceneName.size());

                return packet;
            }

            void fromPacket(ENetPacket* packet) {
                assert(packet->data[0] == MessageType::SetScene);

                uint16_t nameLen = *((uint16_t*)&packet->data[1]); 
                sceneName.resize(nameLen);

                memcpy(sceneName.data(), &packet->data[3], nameLen);
            }
        };
#undef DATAPACKET
#pragma pack (pop)
    }
}
