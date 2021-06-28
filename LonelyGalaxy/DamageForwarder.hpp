#pragma once
#include <entt/entity/fwd.hpp>

namespace lg {
    struct DamageForwarder {
        entt::entity target;
        double multiplier = 1.0;
    };
}
