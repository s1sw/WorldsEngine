#pragma once
#include <cstdint>

struct WrenHandle;

namespace worlds {
    typedef uint32_t AssetID;

    struct ScriptComponent {
        ScriptComponent(AssetID script)
            : script{ script }
            , onSimulate{ nullptr }
            , onUpdate{ nullptr }
            , onStart{ nullptr } {}
        AssetID script;
    private:
        friend class WrenScriptEngine;
        WrenHandle* onStart;
        WrenHandle* onSimulate;
        WrenHandle* onUpdate;
    };
}
