#pragma once
#include <stdint.h>

namespace lg {
    enum class DeathBehaviour {
        Destroy,
        Notify,
        Nothing
    };

    struct RPGStats {
        double maxHP = 100;
        double currentHP = 100;
        uint64_t level = 1;
        uint64_t totalExperience = 0;
        uint8_t strength = 1;
        uint8_t speed = 1;
        uint8_t intelligence = 1;
        DeathBehaviour deathBehaviour = DeathBehaviour::Destroy;

        void damage(double damageAmt) {
            if (damageAmt > currentHP)
                currentHP = 0.0;
            else
                currentHP -= damageAmt;
        }
    };
}
