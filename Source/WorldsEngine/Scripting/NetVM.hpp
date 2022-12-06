#pragma once
#include "Core/IGameEventHandler.hpp"
#include <entt/entity/fwd.hpp>
#include <nlohmann/json_fwd.hpp>

namespace slib
{
    class DynamicLibrary;
}

namespace worlds
{
    class Editor;
    struct DotNetFunctionPtrs
    {
#ifndef __CORECLR_HOST_H__
        void* init;
        void* createDelegate;
        void* shutdown;
#else
        coreclr_initialize_ptr init;
        coreclr_create_delegate_ptr createDelegate;
        coreclr_shutdown_ptr shutdown;
#endif
    };

    struct PhysicsContactInfo;

    class DotNetScriptEngine
    {
      public:
        DotNetScriptEngine(entt::registry& reg, const EngineInterfaces& interfaces);
        bool initialise(Editor* editor);
        void shutdown();
        void onSceneStart();
        void onUpdate(float deltaTime, float interpAlpha);
        void onEditorUpdate(float deltaTime);
        void onSimulate(float deltaTime);
        void handleCollision(entt::entity entity, PhysicsContactInfo* contactInfo);
        void serializeManagedComponents(nlohmann::json& entityJson, entt::entity entity);
        void deserializeManagedComponent(const char* id, const nlohmann::json& componentJson, entt::entity entity);
        void copyManagedComponents(entt::entity from, entt::entity to);
        void createManagedDelegate(const char* typeName, const char* methodName, void** func);

      private:
        const EngineInterfaces& interfaces;
        entt::registry& reg;
        void onTransformDestroy(entt::registry& reg, entt::entity ent);
        void* hostHandle;
        unsigned int domainId;
        DotNetFunctionPtrs netFuncs;
        void (*updateFunc)(float deltaTime, float interpAlpha);
        void (*simulateFunc)(float deltaTime);
        void (*editorUpdateFunc)(float deltaTime);
        void (*nativeEntityDestroyFunc)(uint32_t id);
        void (*serializeComponentsFunc)(void* serializationContext, uint32_t entity);
        void (*deserializeComponentFunc)(const char* id, const nlohmann::json* componentJson, uint32_t entity);
        void (*physicsContactFunc)(uint32_t id, PhysicsContactInfo* contactInfo);
        void (*copyManagedComponentsFunc)(entt::entity from, entt::entity to);
        void (*sceneStartFunc)();
        slib::DynamicLibrary* coreclrLib;
    };
}
