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
            void fireEvent(entt::entity scriptEnt, const char* name);
        private:
            EngineInterfaces interfaces;
            void setupBindings();
            void* hostHandle;
            unsigned int domainId;
            DotNetFunctionPtrs netFuncs;
            void(*updateFunc)(float deltaTime);
    };
}
