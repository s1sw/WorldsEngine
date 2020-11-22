#pragma once
extern "C" {
#include <lua.h>
}
#include <cstdint>
#include <string>
#include <set>
#include <entt/entt.hpp>

namespace worlds {
    class LuaVM {
    public:
        LuaVM();
        void createScriptEnv(uint32_t envName);
        void destroyScriptEnv(uint32_t envName);
        void executeString(std::string str, uint32_t env);
        void bindRegistry(entt::registry& registry);
        ~LuaVM();
    private:
        void loadScriptRunner();
        // pushes a globals table onto the stack for scripts to use
        void pushNewScriptEnv(uint32_t envName);
        lua_State* state;
        std::set<uint32_t> envs;
    };
}