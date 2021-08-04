#pragma once
#include <entt/entity/fwd.hpp>
#include <nlohmann/json_fwd.hpp>
#include "Core/IGameEventHandler.hpp"

namespace worlds {
    class Editor;
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
            bool initialise(Editor* editor);
            void shutdown();
            void onSceneStart();
            void onUpdate(float deltaTime);
            void onEditorUpdate(float deltaTime);
            void onSimulate(float deltaTime);
            void fireEvent(entt::entity scriptEnt, const char* event);
            void serializeManagedComponents(nlohmann::json& entityJson, entt::entity entity);
            void deserializeManagedComponent(const char* id, nlohmann::json& componentJson, entt::entity entity);
        private:
            EngineInterfaces interfaces;
            entt::registry& reg;
            void onTransformDestroy(entt::registry& reg, entt::entity ent);
            void createManagedDelegate(const char* typeName, const char* methodName, void** func);
            void* hostHandle;
            unsigned int domainId;
            DotNetFunctionPtrs netFuncs;
            void(*updateFunc)(float deltaTime);
            void(*simulateFunc)(float deltaTime);
            void(*editorUpdateFunc)(float deltaTime);
            void(*nativeEntityDestroyFunc)(uint32_t id);
            void(*serializeComponentsFunc)(void* serializationContext, uint32_t entity);
            void(*deserializeComponentFunc)(const char* id, const char* componentJson, uint32_t entity);
            void(*sceneStartFunc)();
    };
}
