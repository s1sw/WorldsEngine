#pragma once
#include <wren.hpp>
#include <entt/fwd.hpp>

namespace worlds {
    class WrenScriptEngine {
    public:
        WrenScriptEngine();
        void bindRegistry(entt::registry& reg);
        void onSimulate(entt::registry& reg, float deltaTime);
        void onUpdate(entt::registry& reg, float deltaTime);
        ~WrenScriptEngine();
    private:
        WrenVM* vm;
        WrenHandle* callArgCount[4];
        entt::registry* regPtr;
        static void scriptEntityGetTransform(WrenVM* vm);
        static void scriptTransformGetPosition(WrenVM* vm);
        static void scriptTransformSetPosition(WrenVM* vm);
        void onScriptConstruct(entt::registry&, entt::entity);
        void onScriptDestroy(entt::registry&, entt::entity);
        static WrenForeignMethodFn bindForeignMethod(WrenVM* vm,
            const char* mod,
            const char* className,
            bool isStatic,
            const char* signature);
    };
}
