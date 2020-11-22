#include "LuaVM.hpp"
extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}
#include "Console.hpp"
#include "Log.hpp"
#include <physfs.h>
#include <cassert>
#include <entt/entt.hpp>
#include "Fatal.hpp"
#include "Transform.hpp"
#include "PhysicsActor.hpp"
#include "Physics.hpp"

namespace worlds {
    entt::registry* registryPtr;

    static int printHandler(lua_State* L) {
        int nargs = lua_gettop(L);
        std::string msg{};
        for (int i = 1; i <= nargs; ++i) {
            const char* str = lua_tostring(L, i);

            if (str) {
                msg += str;
            } else {
                msg += lua_typename(L, i);
            }
        }
        
        logMsg("lua: %s", msg.c_str());

        return 0;
    }

    static int lLogError(lua_State* L) {
        int nargs = lua_gettop(L);
        std::string msg{};
        for (int i = 1; i <= nargs; ++i) {
            const char* str = lua_tostring(L, i);

            if (str) {
                msg += str;
            } else {
                msg += lua_typename(L, i);
            }
        }

        logErr("lua: %s", msg.c_str());

        return 0;
    }

    int lCreateEntity(lua_State* L) {
        lua_pushinteger(L, (std::underlying_type<entt::entity>::type)registryPtr->create());
        return 1;
    }

    int lDestroyEntity(lua_State* L) {
        int nargs = lua_gettop(L);
        if (nargs != 1) {
            logErr(WELogCategoryScripting, "called destroyEntity with invalid number of args: %i", nargs);
        }

        uint32_t entity = lua_tointeger(L, 1);
        lua_pop(L, 1);

        registryPtr->destroy((entt::entity)entity);

        return 0;
    }

    void pushVec3(lua_State* L, glm::vec3 v3) {
        lua_createtable(L, 0, 3);
        
        lua_pushstring(L, "x");
        lua_pushnumber(L, v3.x);
        lua_settable(L, -3);
        
        lua_pushstring(L, "y");
        lua_pushnumber(L, v3.y);
        lua_settable(L, -3);

        lua_pushstring(L, "z");
        lua_pushnumber(L, v3.z);
        lua_settable(L, -3);
    }

    void pushQuat(lua_State* L, glm::quat q) {
        lua_createtable(L, 0, 4);

        lua_pushstring(L, "w");
        lua_pushnumber(L, q.w);
        lua_settable(L, -3);

        lua_pushstring(L, "x");
        lua_pushnumber(L, q.x);
        lua_settable(L, -3);

        lua_pushstring(L, "y");
        lua_pushnumber(L, q.y);
        lua_settable(L, -3);

        lua_pushstring(L, "z");
        lua_pushnumber(L, q.z);
        lua_settable(L, -3);
    }

    int lGetTransformPosition(lua_State* L) {
        int nargs = lua_gettop(L);
        if (nargs != 1) {
            logErr(WELogCategoryScripting, "called getTransformPosition with invalid number of args: %i", nargs);
        }

        entt::entity entity = (entt::entity)lua_tointeger(L, 1);
        lua_pop(L, 1);

        Transform& t = registryPtr->get<Transform>(entity);
        pushVec3(L, t.position);

        return 1;
    }

    int lSetTransformPosition(lua_State* L) {
        int nargs = lua_gettop(L);

        if (nargs != 2) {
            logErr(WELogCategoryScripting, "called setTransformPosition with invalid number of args: %i", nargs);
        }

        entt::entity entity = (entt::entity)lua_tointeger(L, 1);
        //lua_pop(L, 1);

        Transform& t = registryPtr->get<Transform>(entity);

        glm::vec3 v;
        lua_getfield(L, -1, "y");
        v.x = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "y");
        v.y = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "z");
        v.z = lua_tonumber(L, -1);
        lua_pop(L, 1);

        t.position = v;

        return 0;
    }

    int lAddRigidbodyForce(lua_State* L) {
        int nargs = lua_gettop(L);

        if (nargs != 2) {
            logErr(WELogCategoryScripting, "called addRigidbodyForce with invalid number of args: %i", nargs);
        }

        entt::entity entity = (entt::entity)lua_tointeger(L, 1);
        //lua_pop(L, 1);

        DynamicPhysicsActor& t = registryPtr->get<DynamicPhysicsActor>(entity);

        glm::vec3 v;
        lua_getfield(L, -1, "y");
        v.x = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "y");
        v.y = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "z");
        v.z = lua_tonumber(L, -1);
        lua_pop(L, 1);

        ((physx::PxRigidBody*)t.actor)->addForce(glm2px(v));

        return 0;
    }

    int lAddEventListener(lua_State* L) {
        return 0;
    }

    static const luaL_Reg defaultEnv[] = {
        {"print", printHandler},
        {"createEntity", lCreateEntity},
        {"destroyEntity", lDestroyEntity},
        {"getTransformPosition", lGetTransformPosition},
        {"setTransformPosition", lSetTransformPosition},
        {"addRigidbodyForce", lAddRigidbodyForce},
        {"logError", lLogError},
        {NULL, NULL} /* end of array */
    };

    const uint32_t nullEnv = entt::entt_traits<std::uint32_t>::entity_mask;

    LuaVM::LuaVM() {
        state = luaL_newstate();

        // add some basic modules
        luaopen_base(state);
        luaopen_math(state);
        luaopen_string(state);
        luaopen_utf8(state);

        // setup our functions for the script runner
        lua_getglobal(state, "_G");
        luaL_setfuncs(state, defaultEnv, 0);
        lua_pop(state, 1);

        loadScriptRunner();

        // create the console script environment
        createScriptEnv(nullEnv);

        g_console->registerCommand([&](void*, const char* arg) {
            // calls run(arg, "exec", "console")
            lua_getglobal(state, "run");
            assert(lua_isfunction(state, -1));
            lua_pushstring(state, arg);
            lua_pushstring(state, "exec");
            lua_pushinteger(state, nullEnv);

            if (lua_pcall(state, 3, 1, 0) != 0) {
                logErr("Error running script runner: %s", lua_tostring(state, -1));
            }

            lua_pop(state, lua_gettop(state));
            }, "execClientLua", "Executes a string of Lua code.", nullptr);

        g_console->registerCommand([&](void*, const char* arg) {
            std::string scriptPath = std::string{ "Scripts/" } + arg;
            PHYSFS_File* file = PHYSFS_openRead(scriptPath.c_str());

            if (file == nullptr) {
                logErr("Failed to open script %s", arg);
                return;
            }

            size_t fileLen = PHYSFS_fileLength(file);
            char* buf = (char*)std::malloc(PHYSFS_fileLength(file) + 1);
            PHYSFS_readBytes(file, buf, fileLen);
            PHYSFS_close(file);
            buf[fileLen] = 0;

            // calls run(buf, arg, "console")
            lua_getglobal(state, "run");
            assert(lua_isfunction(state, -1));
            lua_pushstring(state, buf);
            lua_pushstring(state, arg);
            lua_pushinteger(state, nullEnv);
            if (lua_pcall(state, 3, 1, 0) != 0) {
                logErr("Error running script runner: %s", lua_tostring(state, -1));
            }
            lua_pop(state, lua_gettop(state));

            std::free(buf);
            }, "execClientScript", "Executes a Lua script.", nullptr);
    }

    void LuaVM::createScriptEnv(uint32_t envName) {
        if (envs.contains(envName)) {
            logErr(WELogCategoryScripting, "Tried to create existing script environment!");
            return;
        }
        // calls storeEnv(script environment, envName)
        lua_getglobal(state, "storeEnv");
        assert(lua_isfunction(state, -1));
        pushNewScriptEnv(envName);
        lua_pushinteger(state, envName);
        lua_pcall(state, 2, 1, 0);
        lua_pop(state, lua_gettop(state));

        envs.insert(envName);
    }

    void LuaVM::destroyScriptEnv(uint32_t envName) {
        // calls delEnv(envName)
        lua_getglobal(state, "delEnv");
        assert(lua_isfunction(state, -1));
        lua_pushinteger(state, envName);
        lua_pcall(state, 1, 1, 0);
        lua_pop(state, lua_gettop(state));

        envs.erase(envName);
    }

    void LuaVM::executeString(std::string str, uint32_t env) {
        // calls run(str, "LuaVM::executeString", env)
        lua_getglobal(state, "run");
        assert(lua_isfunction(state, -1));
        lua_pushstring(state, str.c_str());
        lua_pushstring(state, "LuaVM::executeString");
        lua_pushinteger(state, env);

        if (lua_pcall(state, 3, 1, 0) != 0) {
            logErr(WELogCategoryScripting, "Error running script runner: %s", lua_tostring(state, -1));
        }

        lua_pop(state, lua_gettop(state));
    }

    void LuaVM::bindRegistry(entt::registry& registry) {
        registryPtr = &registry;
    }

    void LuaVM::pushNewScriptEnv(uint32_t envName) {
        int numFuncs = (sizeof(defaultEnv) / sizeof(defaultEnv[0])) - 1;
        lua_createtable(state, 0, numFuncs);
        luaL_setfuncs(state, defaultEnv, 0);

        lua_pushstring(state, "envName");
        lua_pushinteger(state, envName);
        lua_settable(state, -3);
    }

    void LuaVM::loadScriptRunner() {
        PHYSFS_File* file = PHYSFS_openRead("Scripts/base_script_loader.lua");

        if (file == nullptr) {
            fatalErr("Failed to open script runner");
            return;
        }

        size_t fileLen = PHYSFS_fileLength(file);
        char* buf = (char*)std::malloc(PHYSFS_fileLength(file));
        PHYSFS_readBytes(file, buf, fileLen);
        PHYSFS_close(file);

        luaL_loadbuffer(state, buf, fileLen, "Script Runner");
        lua_pcall(state, 0, LUA_MULTRET, 0);

        std::free(buf);
    }

    LuaVM::~LuaVM() {
        lua_close(state);
    }
}