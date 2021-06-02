#pragma once
#include <stdint.h>

namespace lg {
    struct RPGStats {
        uint64_t maxHP = 100;
        uint64_t currentHP = 100;
        uint64_t level = 1;
        uint64_t totalExperience = 0;
        uint8_t strength = 1;
        uint8_t speed = 1;
        uint8_t intelligence = 1;

        void damage(uint64_t damageAmt) {
            if (damageAmt > currentHP)
                currentHP = 0;
            else
                currentHP -= damageAmt;
        }
    };
}
