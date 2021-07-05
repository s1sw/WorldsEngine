#pragma once
#include <cstdint>

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        ScriptComponent(AssetID script)
            : script{ script } {}
        AssetID script;
    };
}
