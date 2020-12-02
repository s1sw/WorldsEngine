#pragma once
#include <stdint.h>

namespace converge {
    enum MessageType {
        Msg_Join
    };

    struct NetworkMessage {
        uint8_t msgType;
        void* data;
        uint32_t dataLen;
    };
}
