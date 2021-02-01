#pragma once
#include <cstdint>

struct WrenHandle;

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        ScriptComponent(AssetID script)
            : script{ script }
            , onSimulate{ nullptr }
            , onUpdate{ nullptr }{}
        AssetID script;
    private:
        friend class WrenScriptEngine;
        WrenHandle* onSimulate;
        WrenHandle* onUpdate;
    };
}
