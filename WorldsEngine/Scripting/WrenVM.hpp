#pragma once
#include "../Core/IGameEventHandler.hpp"
#include <string>
#include <wren.hpp>
#include <entt/fwd.hpp>
#include "ScriptComponent.hpp"
#include "../Util/StaticLinkedList.hpp"
#include <robin_hood.h>

namespace worlds {
    class WrenScriptEngine;
    struct WrenVMData {
        entt::registry& reg;
        WrenScriptEngine* scriptEngine;
        EngineInterfaces interfaces;
    };

    class ScriptBindClass {
    public:
        ScriptBindClass();
        virtual std::string getName() = 0;
        virtual WrenForeignMethodFn getFn(bool isStatic, const char* sig) = 0;
        virtual WrenForeignClassMethods getClassMethods() { return WrenForeignClassMethods{}; }
        virtual ~ScriptBindClass() {};
    private:
        static StaticLinkedList<ScriptBindClass> bindClasses;
        friend class WrenScriptEngine;
    };

    class WrenScriptEngine {
    public:
        WrenScriptEngine(entt::registry& reg, EngineInterfaces interfaces);
        void onSceneStart();
        void onSimulate(float deltaTime);
        void onUpdate(float deltaTime);
        void fireEvent(entt::entity scriptEnt, const char* name);
        ~WrenScriptEngine();
    private:
        WrenVM* vm;
        WrenHandle* callArgCount[4];
        WrenVMData* vmDat;
        entt::registry& reg;
        static void scriptEntityGetTransform(WrenVM* vm);
        static void scriptTransformGetPosition(WrenVM* vm);
        static void scriptTransformSetPosition(WrenVM* vm);
        void onScriptConstruct(entt::registry&, entt::entity);
        void onScriptDestroy(entt::registry&, entt::entity);
        void onScriptUpdate(entt::registry&, entt::entity);
        void updateScriptComponent(entt::entity, ScriptComponent& sc);
        static WrenForeignMethodFn bindForeignMethod(
            WrenVM* vm,
            const char* mod,
            const char* className,
            bool isStatic,
            const char* signature);
        robin_hood::unordered_flat_map<std::string, ScriptBindClass*> scriptBindings;
    };
}
