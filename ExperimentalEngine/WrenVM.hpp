#pragma once
#include <wren.hpp>
#include <entt/fwd.hpp>

namespace worlds {
    class WrenScriptEngine {
    public:
        WrenScriptEngine();
        void bindRegistry(entt::registry& reg);
        ~WrenScriptEngine();
    private:
        WrenVM* vm;
        entt::registry* regPtr;
        static void scriptEntityGetTransform(WrenVM* vm);
        static void scriptTransformGetPosition(WrenVM* vm);
        static void scriptTransformSetPosition(WrenVM* vm);
        static WrenForeignMethodFn bindForeignMethod(WrenVM* vm,
            const char* mod,
            const char* className,
            bool isStatic,
            const char* signature);
    };
}