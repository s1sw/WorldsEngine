#pragma once
#include "Core/Transform.hpp"

namespace lg {
    struct Gun {
        // Editable
        Transform firePoint;
        float shotPeriod = 0.1f;
        bool automatic = false;

        // Not Editable
        double lastFireTime = 0.0;
    };
}
