#include "WrenVM.hpp"
#include "Log.hpp"
#include "Console.hpp"
#include <physfs.h>
#include "IOUtil.hpp"
#include <entt/entt.hpp>
#include "Transform.hpp"
#include "ScriptComponent.hpp"
#include "entt/entity/fwd.hpp"
#include "AssetDB.hpp"
#include "wren.h"

namespace worlds {
    void writeFn(WrenVM* vm, const char* text) {
        if (strcmp(text, "\n") == 0) return;
        logMsg(WELogCategoryScripting, "%s", text);
    }

    void errorFn(WrenVM* vm, WrenErrorType errorType,
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

    void makeVec3(WrenVM* vm, float x, float y, float z) {
        wrenEnsureSlots(vm, 1);
        wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);

        glm::vec3* vPtr = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
        *vPtr = glm::vec3{ x, y, z };
    }

    void WrenScriptEngine::scriptEntityGetTransform(WrenVM* vm) {
        uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

        wrenEnsureSlots(vm, 1);
        wrenGetVariable(vm, "worlds_engine/entity", "Transform", 0);
        WrenScriptEngine* _this = (WrenScriptEngine*)wrenGetUserData(vm);
        
        uint32_t* idPtr = (uint32_t*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(uint32_t));
        *idPtr = entId;
    }

    void WrenScriptEngine::scriptTransformGetPosition(WrenVM* vm) {
        wrenEnsureSlots(vm, 1);
        WrenScriptEngine* _this = (WrenScriptEngine*)wrenGetUserData(vm);

        uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

        Transform& t = _this->regPtr->get<Transform>((entt::entity)entId);

        makeVec3(vm, t.position.x, t.position.y, t.position.z);
    }

    void WrenScriptEngine::scriptTransformSetPosition(WrenVM* vm) {
        wrenEnsureSlots(vm, 1);
        WrenScriptEngine* _this = (WrenScriptEngine*)wrenGetUserData(vm);

        uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);
        glm::vec3* posPtr = (glm::vec3*)wrenGetSlotForeign(vm, 1);

        Transform& t = _this->regPtr->get<Transform>((entt::entity)entId);

        t.position = *posPtr;
    }

    void scriptVec3GetComp(WrenVM* vm) {
        wrenEnsureSlots(vm, 1);
        glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

        wrenSetSlotDouble(vm, 0, (*vPtr)[wrenGetSlotDouble(vm, 1)]);
    }

    void scriptVec3SetComp(WrenVM* vm) {
        glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

        (*vPtr)[wrenGetSlotDouble(vm, 1)] = wrenGetSlotDouble(vm, 2);
    }

    void scriptVec3SetAll(WrenVM* vm) {
        glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

        for (int i = 0; i < 3; i++) {
            (*vPtr)[i] = wrenGetSlotDouble(vm, i + 1);
        }
    }

    WrenForeignMethodFn WrenScriptEngine::bindForeignMethod(
        WrenVM* vm,
        const char* mod,
        const char* className,
        bool isStatic,
        const char* signature) {

        if (strcmp(className, "Entity") == 0 && !isStatic) {
            if (strcmp(signature, "getTransform()") == 0) {
                return WrenScriptEngine::scriptEntityGetTransform;
            }
        } else if (strcmp(className, "Transform") == 0 && !isStatic) {
            if (strcmp(signature, "getPosition()") == 0) {
                return WrenScriptEngine::scriptTransformGetPosition;
            } else if (strcmp(signature, "setPosition(_)") == 0) {
                return WrenScriptEngine::scriptTransformSetPosition;
            }
        } else if (strcmp(className, "Vec3") == 0 && !isStatic) {
            if (strcmp(signature, "getComp(_)") == 0) {
                return scriptVec3GetComp;
            } else if (strcmp(signature, "setComp(_,_)") == 0) {
                return scriptVec3SetComp;
            } else if (strcmp(signature, "setAll(_,_,_)") == 0) {
                return scriptVec3SetAll;
            }
        }
        return nullptr;
    }

    void entityAllocate(WrenVM* vm) {
        uint32_t* idPtr = (uint32_t*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(uint32_t));

        *idPtr = wrenGetSlotDouble(vm, 1);
    }

    WrenForeignClassMethods bindForeignClass(
        WrenVM* vm, const char* mod, const char* className) {
        WrenForeignClassMethods methods{};

        if (strcmp(className, "Entity") == 0) {
            methods.allocate = entityAllocate;
        } else {
            methods = {};
        }

        return methods;
    }

    WrenLoadModuleResult loadModule(WrenVM* vm, const char* name) {
        WrenLoadModuleResult res{};

        // TODO: Could theoretically read contents of other data files by abusing ..
        // Not an issue since we're assuming that scripts are trusted but oh well
        auto ioRes = LoadFileToString("ScriptModules/" + std::string(name) + ".wren");

        if (ioRes.error != IOError::None) {
            logErr(WELogCategoryScripting, "Failed to load module %s", name);
            return res;
        }

        res.source = strdup(ioRes.value.c_str());

        return res;
    }

    std::string getModuleString(entt::entity ent, AssetID scriptId) {
        return std::to_string((uint32_t)ent) + ":" + std::to_string(scriptId);
    }

    void WrenScriptEngine::onScriptConstruct(entt::registry& reg, entt::entity ent) {
        auto& sc = reg.get<ScriptComponent>(ent);

        auto ioRes = LoadFileToString(g_assetDB.getAssetPath(sc.script));
        
        if (ioRes.error != IOError::None) {
            logErr(WELogCategoryScripting, "Failed to load script");
            return;
        }
        
        // generate module string
        auto modStr = getModuleString(ent, sc.script);
        wrenInterpret(vm, modStr.c_str(), ioRes.value.c_str());
    }

    void WrenScriptEngine::onScriptDestroy(entt::registry& reg, entt::entity ent) {
        // TODO: The current version of Wren we're using doesn't allow
        // unloading/destroying modules. At some point we should probably
        // fix this in our fork, but we don't have the ability to leak anything
        // large right now so it doesn't really matter.
    }

    WrenScriptEngine::WrenScriptEngine() {
        WrenConfiguration config;
        wrenInitConfiguration(&config);
        config.writeFn = writeFn;
        config.errorFn = errorFn;
        config.bindForeignMethodFn = bindForeignMethod;
        config.bindForeignClassFn = bindForeignClass;
        config.loadModuleFn = loadModule;

        vm = wrenNewVM(&config);
        wrenSetUserData(vm, this);

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
    }

    void WrenScriptEngine::bindRegistry(entt::registry& reg) {
        regPtr = &reg;
        reg.on_construct<ScriptComponent>()
            .connect<&WrenScriptEngine::onScriptConstruct>(*this);
        reg.on_destroy<ScriptComponent>()
            .connect<&WrenScriptEngine::onScriptDestroy>(*this);
    }

    void setEntitySlot(entt::entity ent, WrenVM* vm, int slot) {
        wrenGetVariable(vm, "worlds_engine/entity", "Entity", slot);
        uint32_t* idPtr = 
            (uint32_t*)wrenSetSlotNewForeign(vm, slot, slot, sizeof(uint32_t));

        *idPtr = (uint32_t)ent;
    }
    
    void WrenScriptEngine::onSimulate(entt::registry& reg, float deltaTime) {
        reg.view<ScriptComponent>().each([&](entt::entity ent, ScriptComponent& sc) {
            if (!sc.handlesChecked) {
                auto modStr = getModuleString(ent, sc.script);
                wrenGetVariable(vm, modStr.c_str(), "onSimulate", 0);
                if (wrenGetSlotType(vm, 0) != WREN_TYPE_NULL) {
                    sc.onSimulate = wrenGetSlotHandle(vm, 0);
                }
            }

            if (sc.onSimulate) {
                wrenSetSlotHandle(vm, 0, sc.onSimulate);
                setEntitySlot(ent, vm, 1);
                wrenSetSlotDouble(vm, 2, deltaTime);

                wrenCall(vm, callArgCount[2]);
            }
        });
    }

    WrenScriptEngine::~WrenScriptEngine() {
        wrenFreeVM(vm);
    }
}
