#pragma once
#include <entt/entity/fwd.hpp>
#include "Core/IGameEventHandler.hpp"

namespace worlds {
    struct DotNetFunctionPtrs {
#ifndef __CORECLR_HOST_H__
        void* init;
        void* createDeletgate;
        void* shutdown;
#else
        coreclr_initialize_ptr init;
        coreclr_create_delegate_ptr createDelegate;
        coreclr_shutdown_ptr shutdown;
#endif
    };

    class DotNetScriptEngine {
        public:
            DotNetScriptEngine(entt::registry& reg, EngineInterfaces interfaces);
            bool initialise();
            void shutdown();
            void onSceneStart();
            void onUpdate(float deltaTime);
            void onSimulate(float deltaTime);
            void fireEvent(entt::entity scriptEnt, const char* event);
        private:
            EngineInterfaces interfaces;
            entt::registry& reg;
            void createManagedDelegate(const char* typeName, const char* methodName, void** func);
            void* hostHandle;
            unsigned int domainId;
            DotNetFunctionPtrs netFuncs;
            void(*updateFunc)(float deltaTime);
            void(*sceneStartFunc)(entt::registry* registry);
    };
}
