#pragma once
#include <cstdint>

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        AssetID script;
    };
}