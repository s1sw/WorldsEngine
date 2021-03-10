#pragma once
#include <cstdint>

struct WrenHandle;

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        ScriptComponent(AssetID script)
            : script{ script } {}
        AssetID script;
    private:
        friend class WrenScriptEngine;
        WrenHandle* onStart = nullptr;
        WrenHandle* onSimulate = nullptr;
        WrenHandle* onUpdate = nullptr;
    };
}
