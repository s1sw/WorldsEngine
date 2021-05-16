#pragma once
#include <stdint.h>

namespace lg {
    struct ContactDamageDealer {
        uint64_t damage;
        float minVelocity;
        float maxVelocity;
    };
}
