#pragma once
#include <Core/AssetDB.hpp>

namespace lg {
    struct PhysicsSoundComponent {
        worlds::AssetID soundId = ~0u;
        double lastPlayTime = 0.0;
        uint32_t eventHandlerIndex = ~0u;
    };
}
