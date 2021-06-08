#include "WrenVM.hpp"
#include "../Core/Log.hpp"
#include "../Core/Console.hpp"
#include <physfs.h>
#include "../IO/IOUtil.hpp"
#include <entt/entt.hpp>
#include "../Core/Transform.hpp"
#include "ScriptComponent.hpp"
#include "entt/entity/fwd.hpp"
#include "../Core/AssetDB.hpp"
#include "wren.h"
#include "wren.hpp"
#include <string.h>
#include <stdlib.h>

#include "ScriptBindings/Entity.hpp"
#include "ScriptBindings/Transform.hpp"
#include "ScriptBindings/Vec3.hpp"
#include "ScriptBindings/PhysicsActors.hpp"
#include "ScriptBindings/SceneManager.hpp"
#include "ScriptBindings/Light.hpp"
#include "ScriptBindings/Console.hpp"

namespace worlds {
    void writeFn(WrenVM*, const char* text) {
        if (strcmp(text, "\n") == 0) return;
        logMsg(WELogCategoryScripting, "%s", text);
    }

    void errorFn(WrenVM*, WrenErrorType errorType,
        const char* mod, const int line,
        const char* msg) {
        switch (errorType) {
        case WREN_ERROR_COMPILE: {
            logErr(WELogCategoryScripting, "[%s line %d] [Error] %s", mod, line, msg);
        } break;
        case WREN_ERROR_STACK_TRACE: {
            logErr(WELogCategoryScripting, "[%s line %d] in %s", mod, line, msg);
        } break;
        case WREN_ERROR_RUNTIME: {
            logErr(WELogCategoryScripting, "[Runtime Error] %s\n", msg);
        } break;
        }
    }

    WrenForeignMethodFn WrenScriptEngine::bindForeignMethod(
        WrenVM* vm,
        const char* /* mod */,
        const char* className,
        bool isStatic,
        const char* signature) {
        auto* vmDat = (WrenVMData*)wrenGetUserData(vm);
        auto* _this = vmDat->scriptEngine;

        auto it = _this->scriptBindings.find(className);

        if (it == _this->scriptBindings.end())
            return nullptr;

        return it->second->getFn(isStatic, signature);
    }

    void entityAllocate(WrenVM* vm) {
        uint32_t* idPtr = (uint32_t*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(uint32_t));

        *idPtr = (uint32_t)wrenGetSlotDouble(vm, 1);
    }

    void vec3Allocate(WrenVM* vm) {
        glm::vec3* v3 = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
        (*v3) = glm::vec3 {0.0f};
    }

    WrenForeignClassMethods bindForeignClass(
        WrenVM* /* vm */,
        const char* /* mod */,
        const char* className) {
        WrenForeignClassMethods methods{};

        if (strcmp(className, "Entity") == 0) {
            methods.allocate = entityAllocate;
        } else if (strcmp(className, "Vec3") == 0) {
            methods.allocate = vec3Allocate;
        }

        return methods;
    }

    WrenLoadModuleResult loadModule(WrenVM* /* vm */, const char* name) {
        WrenLoadModuleResult res{};

        // TODO: Could theoretically read contents of other data files by abusing ..
        // Not an issue since we're assuming that scripts are trusted but oh well
        auto ioRes = LoadFileToString("ScriptModules/" + std::string(name) + ".wren");

        if (ioRes.error != IOError::None) {
            logErr(WELogCategoryScripting, "Failed to load module %s", name);
            return res;
        }

        res.source = strdup(ioRes.value.c_str());

        res.onComplete = [](WrenVM*, const char*, WrenLoadModuleResult result) {
            free((char*)result.source);
        };

        return res;
    }

    std::string getModuleString(entt::entity ent, AssetID scriptId) {
        return std::to_string((uint32_t)ent) + ":" + std::to_string(scriptId);
    }

    void WrenScriptEngine::onScriptConstruct(entt::registry& reg, entt::entity ent) {
        auto& sc = reg.get<ScriptComponent>(ent);
        updateScriptComponent(ent, sc);
    }

    void WrenScriptEngine::onScriptDestroy(entt::registry& reg, entt::entity ent) {
        auto& sc = reg.get<ScriptComponent>(ent);
        if (sc.onStart)
            wrenReleaseHandle(vm, sc.onStart);

        if (sc.onSimulate)
            wrenReleaseHandle(vm, sc.onSimulate);

        if (sc.onUpdate)
            wrenReleaseHandle(vm, sc.onUpdate);

        auto modStr = getModuleString(ent, sc.script);

        wrenUnloadModule(vm, modStr.c_str());
    }

    void WrenScriptEngine::onScriptUpdate(entt::registry& reg, entt::entity ent) {
        auto& sc = reg.get<ScriptComponent>(ent);
        updateScriptComponent(ent, sc);
    }

    void WrenScriptEngine::updateScriptComponent(entt::entity ent, ScriptComponent& sc) {
        auto ioRes = LoadFileToString(AssetDB::idToPath(sc.script));

        if (ioRes.error != IOError::None) {
            logErr(WELogCategoryScripting, "Failed to load script");
            return;
        }

        auto modStr = getModuleString(ent, sc.script);

        if (wrenHasModule(vm, modStr.c_str())) {
            wrenUnloadModule(vm, modStr.c_str());
        }

        wrenInterpret(vm, modStr.c_str(), ioRes.value.c_str());

        wrenEnsureSlots(vm, 1);

        wrenGetVariable(vm, modStr.c_str(), "onStart", 0);
        if (wrenGetSlotType(vm, 0) != WREN_TYPE_NULL) {
            sc.onStart = wrenGetSlotHandle(vm, 0);
        }

        wrenGetVariable(vm, modStr.c_str(), "onSimulate", 0);
        if (wrenGetSlotType(vm, 0) != WREN_TYPE_NULL) {
            sc.onSimulate = wrenGetSlotHandle(vm, 0);
        }

        wrenGetVariable(vm, modStr.c_str(), "onUpdate", 0);
        if (wrenGetSlotType(vm, 0) != WREN_TYPE_NULL) {
            sc.onUpdate = wrenGetSlotHandle(vm, 0);
        }
    }

    WrenScriptEngine::WrenScriptEngine(entt::registry& reg, EngineInterfaces interfaces)
        : reg{reg} {
        WrenConfiguration config;
        wrenInitConfiguration(&config);
        config.writeFn = writeFn;
        config.errorFn = errorFn;
        config.bindForeignMethodFn = bindForeignMethod;
        config.bindForeignClassFn = bindForeignClass;
        config.loadModuleFn = loadModule;

        interfaces.scriptEngine = this;
        vmDat = new WrenVMData{reg, this, interfaces};

        vm = wrenNewVM(&config);
        wrenSetUserData(vm, vmDat);

        g_console->registerCommand([&](void*, const char* arg) {
            WrenInterpretResult res = wrenInterpret(vm, "console", arg);

            if (res != WREN_RESULT_SUCCESS) {
                logErr(WELogCategoryScripting, "Error when interpreting console script");
            }
            }, "execWren", "Executes a string of Wren code.", nullptr);

        g_console->registerCommand([&](void*, const char* arg) {
            auto res = LoadFileToString(arg);

            if (res.error != IOError::None) {
                logErr(WELogCategoryScripting, "Couldn't load script file");
            } else {
                WrenInterpretResult interpRes = wrenInterpret(vm, arg, res.value.c_str());

                if (interpRes != WREN_RESULT_SUCCESS) {
                    logErr(WELogCategoryScripting, "Error when interpreting script file");
                }
            }
            }, "execWrenScript", "Executes a Wren script file.", nullptr);

        g_console->registerCommand([&](void*, const char*) {
            wrenCollectGarbage(vm);
        }, "forceWrenGC", "Forces Wren to perform a garbage collection.", nullptr);

        for (int i = 0; i < 4; i++) {
            std::string sigStr = "call(";

            for (int j = 0; j < i; j++) {
                sigStr += "_";

                if (j < i - 1) {
                    sigStr += ",";
                }
            }

            sigStr += ")";

            callArgCount[i] = wrenMakeCallHandle(vm, sigStr.c_str());
        }

        reg.on_construct<ScriptComponent>()
            .connect<&WrenScriptEngine::onScriptConstruct>(*this);
        reg.on_destroy<ScriptComponent>()
            .connect<&WrenScriptEngine::onScriptDestroy>(*this);
        reg.on_update<ScriptComponent>()
            .connect<&WrenScriptEngine::onScriptUpdate>(*this);

        // put script bindings into hash map
        auto currNode = ScriptBindClass::bindClasses.first;

        while (currNode != nullptr) {
            scriptBindings.insert({ currNode->ptr->getName(), currNode->ptr });
            currNode = currNode->next;
        }
    }

    void setEntitySlot(entt::entity ent, WrenVM* vm, int slot) {
        wrenGetVariable(vm, "worlds_engine/entity", "Entity", slot);
        uint32_t* idPtr =
            (uint32_t*)wrenSetSlotNewForeign(vm, slot, slot, sizeof(uint32_t));

        *idPtr = (uint32_t)ent;
    }

    void WrenScriptEngine::onSceneStart() {
        reg.view<ScriptComponent>().each([&](entt::entity ent, ScriptComponent& sc) {
            if (sc.onStart) {
                wrenEnsureSlots(vm, 2);
                wrenSetSlotHandle(vm, 0, sc.onStart);
                setEntitySlot(ent, vm, 1);
                wrenCall(vm, callArgCount[1]);
            }
        });
    }

    void WrenScriptEngine::onSimulate(float deltaTime) {
        reg.view<ScriptComponent>().each([&](entt::entity ent, ScriptComponent& sc) {
            if (sc.onSimulate) {
                wrenEnsureSlots(vm, 3);
                wrenSetSlotHandle(vm, 0, sc.onSimulate);
                setEntitySlot(ent, vm, 1);
                wrenSetSlotDouble(vm, 2, deltaTime);

                wrenCall(vm, callArgCount[2]);
            }
        });
    }

    void WrenScriptEngine::onUpdate(float deltaTime) {
        reg.view<ScriptComponent>().each([&](entt::entity ent, ScriptComponent& sc) {
            if (sc.onUpdate) {
                wrenEnsureSlots(vm, 3);
                wrenSetSlotHandle(vm, 0, sc.onUpdate);
                setEntitySlot(ent, vm, 1);
                wrenSetSlotDouble(vm, 2, deltaTime);

                wrenCall(vm, callArgCount[2]);
            }
        });
    }

    void WrenScriptEngine::fireEvent(entt::entity scriptEnt, const char* name) {
        if (!reg.has<ScriptComponent>(scriptEnt)) {
            logErr(WELogCategoryScripting, "Tried to fire event on non-scripted entity!");
            return;
        }

        auto& sc = reg.get<ScriptComponent>(scriptEnt);
        auto modStr = getModuleString(scriptEnt, sc.script);
        wrenGetVariable(vm, modStr.c_str(), name, 0);

        if (wrenGetSlotType(vm, 0) == WREN_TYPE_NULL) {
            return;
        }

        setEntitySlot(scriptEnt, vm, 1);
        wrenCall(vm, callArgCount[1]);
    }

    WrenScriptEngine::~WrenScriptEngine() {
        wrenFreeVM(vm);
    }
}
