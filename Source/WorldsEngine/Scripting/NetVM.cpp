#include <coreclrhost.h>
#include "NetVM.hpp"
#include "Core/Fatal.hpp"
#include "Export.hpp"
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <Core/Transform.hpp>
#include <Core/WorldComponents.hpp>
#include <ImGui/imgui.h>
#include <Serialization/SceneSerialization.hpp>
#include <entt/entity/registry.hpp>
#include <filesystem>
#include <slib/DynamicLibrary.hpp>
#include <string>
#include <Tracy.hpp>

#if defined(_WIN32)
#define NET_LIBRARY_PATH "./NetAssemblies/coreclr.dll"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#define NET_LIBRARY_PATH "NetAssemblies/libcoreclr.so"
#endif

enum CSMessageSeverity
{
    CS_Verbose,
    CS_Info,
    CS_Warning,
    CS_Error
};

using namespace worlds;
extern "C"
{
    EXPORT void logging_log(CSMessageSeverity severity, const char* text)
    {
        switch (severity)
        {
        case CS_Verbose:
            logVrb(worlds::WELogCategoryScripting, "%s", text);
            break;
        case CS_Info:
            logMsg(worlds::WELogCategoryScripting, "%s", text);
            break;
        case CS_Warning:
            logWarn(worlds::WELogCategoryScripting, "%s", text);
            break;
        case CS_Error:
            logErr(worlds::WELogCategoryScripting, "%s", text);
            break;
        }
    }
}

EngineInterfaces const* csharpInterfaces;
#include "AssetDBBindings.hpp"
#include "AudioBindings.hpp"
#include "AudioSourceBindings.hpp"
#include "CameraBindings.hpp"
#include "ComponentMetadataBindings.hpp"
#include "ConsoleBindings.hpp"
#include "DebugShapeBindings.hpp"
#include "DynamicPhysicsActorBindings.hpp"
#include "ImGui/cimgui.h"
#include "InputBindings.hpp"
#include "MeshManagerBindings.hpp"
#include "NavigationSystemBindings.hpp"
#include "PhysicsBindings.hpp"
#include "RegistryBindings.hpp"
#include "SkinnedWorldObjectBindings.hpp"
#include "VRBindings.hpp"
#include "WorldLightBindings.hpp"
#include "WorldObjectBindings.hpp"
#include "WorldTextBindings.hpp"
#include "NMJsonBindings.hpp"

#ifdef BUILD_EDITOR
#include "EditorBindings.hpp"
#include "GameProjectBindings.hpp"
#endif

entt::registry* sceneLoaderBindReg;
extern "C"
{
    EXPORT void sceneloader_loadScene(AssetID id)
    {
        csharpInterfaces->engine->loadScene(id);
    }

    EXPORT AssetID sceneloader_getCurrentSceneID()
    {
        return sceneLoaderBindReg->ctx<SceneInfo>().id;
    }
}

namespace worlds
{
    DotNetScriptEngine::DotNetScriptEngine(entt::registry& reg, const EngineInterfaces& interfaces)
        : interfaces(interfaces), reg(reg)
    {
        csharpInputManager = interfaces.inputManager;
        csharpVrInterface = interfaces.vrInterface;
        csharpInterfaces = &interfaces;
    }

    bool DotNetScriptEngine::initialise(Editor* editor)
    {
        ZoneScoped;
        // Force the cimgui functions to be compiled in
        igGET_FLT_MAX();
        coreclrLib = new slib::DynamicLibrary(NET_LIBRARY_PATH);

        if (!coreclrLib->loaded())
        {
            logErr("Failed to load coreclr");
            return false;
        }

        netFuncs.init = (coreclr_initialize_ptr)coreclrLib->getFunctionPointer("coreclr_initialize");
        netFuncs.createDelegate =
            (coreclr_create_delegate_ptr)coreclrLib->getFunctionPointer("coreclr_create_delegate");
        netFuncs.shutdown = (coreclr_shutdown_ptr)coreclrLib->getFunctionPointer("coreclr_shutdown");

        if (netFuncs.init == NULL || netFuncs.createDelegate == NULL || netFuncs.shutdown == NULL)
        {
            logErr("Failed to get .net functions");
            return false;
        }

#if defined(_WIN32)
        char exePath[MAX_PATH];
        GetModuleFileNameA(0, exePath, MAX_PATH);
#elif defined(__linux__)
        char exePath[PATH_MAX];
        ssize_t readChars = readlink("/proc/self/exe", exePath, PATH_MAX);
        exePath[readChars] = 0;
#endif

        logVrb("exePath: %s", exePath);

        const char* propertyKeys[] = {"TRUSTED_PLATFORM_ASSEMBLIES"};

        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

        std::string tpaList = "";

        for (const auto& dirEntry : recursive_directory_iterator(std::filesystem::path(exePath).parent_path() / "NetAssemblies"))
        {
            if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".dll")
            {
                tpaList += (dirEntry.path()).string();
#if defined(_WIN32)
                tpaList += ";";
#elif defined(__linux__)
                tpaList += ":";
#endif
            }
        }

        const char* propertyValues[] = {tpaList.c_str()};

        csharpEditor = editor;
        csharpMainCamera = interfaces.mainCamera;
        sceneLoaderBindReg = &reg;

        int ret = netFuncs.init(exePath, "WorldsEngine", 1, propertyKeys, propertyValues, &hostHandle, &domainId);

        if (ret < 0)
        {
            logErr("Failed to initialise coreclr: 0x%x", ret);
            return false;
        }

        // C# bools are always 4 bytes
        uint32_t (*initFunc)(entt::registry * reg, uint32_t editorActive, uint32_t isDebug);
        createManagedDelegate("WorldsEngine.Engine", "Init", (void**)&initFunc);

        bool isDebug = true;
#ifdef NDEBUG
        isDebug = false;
#endif

        if (!initFunc(&reg, interfaces.engine->runAsEditor, (uint32_t)isDebug))
            return false;

        createManagedDelegate("WorldsEngine.Engine", "Update", (void**)&updateFunc);
        createManagedDelegate("WorldsEngine.Engine", "Simulate", (void**)&simulateFunc);
        createManagedDelegate("WorldsEngine.Engine", "OnSceneStart", (void**)&sceneStartFunc);
        createManagedDelegate("WorldsEngine.Engine", "EditorUpdate", (void**)&editorUpdateFunc);
        createManagedDelegate("WorldsEngine.ECS.Registry", "OnNativeEntityDestroy", (void**)&nativeEntityDestroyFunc);
        createManagedDelegate("WorldsEngine.ECS.Registry", "SerializeManagedComponents", (void**)&serializeComponentsFunc);
        createManagedDelegate("WorldsEngine.ECS.Registry", "DeserializeManagedComponent",
                              (void**)&deserializeComponentFunc);
        createManagedDelegate("WorldsEngine.ECS.Registry", "CopyManagedComponents", (void**)&copyManagedComponentsFunc);
        createManagedDelegate("WorldsEngine.Physics", "HandleCollisionFromNative", (void**)&physicsContactFunc);

        reg.on_destroy<Transform>().connect<&DotNetScriptEngine::onTransformDestroy>(*this);

        JsonSceneSerializer::setScriptEngine(this);

        return true;
    }

    void DotNetScriptEngine::shutdown()
    {
        reg.on_destroy<Transform>().disconnect<&DotNetScriptEngine::onTransformDestroy>(*this);
        netFuncs.shutdown(hostHandle, domainId);
    }

    void DotNetScriptEngine::onTransformDestroy(entt::registry& reg, entt::entity ent)
    {
        nativeEntityDestroyFunc((uint32_t)ent);
    }

    void DotNetScriptEngine::onSceneStart()
    {
        ZoneScoped;

        sceneStartFunc();
    }

    void DotNetScriptEngine::onUpdate(float deltaTime, float interpAlpha)
    {
        ZoneScoped;

        updateFunc(deltaTime, interpAlpha);
    }

    void DotNetScriptEngine::onEditorUpdate(float deltaTime)
    {
        ZoneScoped;

        editorUpdateFunc(deltaTime);
    }

    void DotNetScriptEngine::onSimulate(float deltaTime)
    {
        ZoneScoped;

        simulateFunc(deltaTime);
    }

    void DotNetScriptEngine::handleCollision(entt::entity entity, PhysicsContactInfo* contactInfo)
    {
        physicsContactFunc((uint32_t)entity, contactInfo);
    }

    void DotNetScriptEngine::serializeManagedComponents(nlohmann::json& entityJson, entt::entity entity)
    {
        serializeComponentsFunc((void*)&entityJson, (uint32_t)entity);
    }

    void DotNetScriptEngine::deserializeManagedComponent(const char* id, const nlohmann::json& componentJson,
                                                         entt::entity entity)
    {
        ZoneScoped;
        std::string cJsonStr = componentJson.dump();
        deserializeComponentFunc(id, &componentJson, (uint32_t)entity);
    }

    void DotNetScriptEngine::copyManagedComponents(entt::entity from, entt::entity to)
    {
        copyManagedComponentsFunc(from, to);
    }

    void DotNetScriptEngine::createManagedDelegate(const char* typeName, const char* methodName, void** func)
    {
        int ret = netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", typeName, methodName, func);
        if (ret < 0)
        {
            std::string str = "Failed to create delegate ";
            str += typeName;
            str += ".";
            str += methodName;
            fatalErr(str.c_str());
        }
    }
}
