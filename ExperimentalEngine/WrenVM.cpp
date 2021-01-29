#include "WrenVM.hpp"
#include "Log.hpp"
#include "Console.hpp"
#include <physfs.h>
#include "IOUtil.hpp"
#include <entt/entt.hpp>
#include "Transform.hpp"
#include "ScriptComponent.hpp"

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

    char* loadModule(WrenVM* vm, const char* name) {
        // TODO: Could theoretically read contents of other data files by abusing ..
        // Not an issue since we're assuming that scripts are trusted but oh well
        auto ioRes = LoadFileToString("ScriptModules/" + std::string(name) + ".wren");

        if (ioRes.error != IOError::None) {
            logErr(WELogCategoryScripting, "Failed to load module %s", name);
            return nullptr;
        }

        return _strdup(ioRes.value.c_str());
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
    }

    void WrenScriptEngine::bindRegistry(entt::registry& reg) {
        regPtr = &reg;
        reg.on_construct<ScriptComponent>().connect
    }

    WrenScriptEngine::~WrenScriptEngine() {
        wrenFreeVM(vm);
    }
}