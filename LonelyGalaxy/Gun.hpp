#pragma once
#include "Core/Transform.hpp"

namespace lg {
    struct Gun {
        // Editable
        Transform firePoint;
        float shotPeriod = 0.1f;
        bool automatic = false;
        double damage = 15.0;

        // Not Editable
        double lastFireTime = 0.0;
    };
}
