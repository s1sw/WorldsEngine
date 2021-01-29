#pragma once
#include <cstdint>

struct WrenHandle;

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        ScriptComponent() : handlesChecked{false} {}
        AssetID script;
    private:
        friend class WrenScriptEngine;
        bool handlesChecked;
        WrenHandle* onSimulate;
    };
}
