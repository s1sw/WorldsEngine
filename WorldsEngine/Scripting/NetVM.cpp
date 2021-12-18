#include <coreclrhost.h>
#include "Core/Fatal.hpp"
#include "NetVM.hpp"
#include <string>
#include <filesystem>
#include <ImGui/imgui.h>
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Core/Transform.hpp>
#include <Core/NameComponent.hpp>
#include <entt/entity/registry.hpp>
#include "Export.hpp"
#include <Serialization/SceneSerialization.hpp>
#include <tracy/Tracy.hpp>
#include <slib/DynamicLibrary.hpp>

#if defined(_WIN32)
#define NET_LIBRARY_PATH "./NetAssemblies/coreclr.dll"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__linux__)
#define NET_LIBRARY_PATH "NetAssemblies/libcoreclr.so"
#endif

enum CSMessageSeverity {
    CS_Verbose,
    CS_Info,
    CS_Warning,
    CS_Error
};

using namespace worlds;
extern "C" {
    EXPORT void logging_log(CSMessageSeverity severity, const char* text) {
        switch (severity) {
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

EngineInterfaces csharpInterfaces;
#include "RegistryBindings.hpp"
#include "WorldObjectBindings.hpp"
#include "AssetDBBindings.hpp"
#include "CameraBindings.hpp"
#include "InputBindings.hpp"
#include "ConsoleBindings.hpp"
#include "ComponentMetadataBindings.hpp"
#include "EditorBindings.hpp"
#include "DynamicPhysicsActorBindings.hpp"
#include "PhysicsBindings.hpp"
#include "AudioBindings.hpp"
#include "AudioSourceBindings.hpp"
#include "VRBindings.hpp"
#include "WorldLightBindings.hpp"
#include "ImGui/cimgui.h"
#include "SkinnedWorldObjectBindings.hpp"
#include "MeshManagerBindings.hpp"
#include "WorldTextBindings.hpp"

extern "C" {
    EXPORT void sceneloader_loadScene(AssetID id) {
        csharpInterfaces.engine->loadScene(id);
    }

    EXPORT AssetID sceneloader_getCurrentSceneID() {
        return csharpInterfaces.engine->getCurrentSceneInfo().id;
    }
}

namespace worlds {
    DotNetScriptEngine::DotNetScriptEngine(entt::registry& reg, EngineInterfaces interfaces)
        : interfaces(interfaces)
        , reg(reg) {
        csharpInputManager = interfaces.inputManager;
        csharpVrInterface = interfaces.vrInterface;
        csharpInterfaces = interfaces;
    }

    bool DotNetScriptEngine::initialise(Editor* editor) {
        ZoneScoped;
        igGET_FLT_MAX();
        coreclrLib = new slib::DynamicLibrary(NET_LIBRARY_PATH);

        if (!coreclrLib->loaded()) {
            logErr("Failed to load coreclr");
            return false;
        }

        netFuncs.init = (coreclr_initialize_ptr)coreclrLib->getFunctionPointer("coreclr_initialize");
        netFuncs.createDelegate = (coreclr_create_delegate_ptr)coreclrLib->getFunctionPointer("coreclr_create_delegate");
        netFuncs.shutdown = (coreclr_shutdown_ptr)coreclrLib->getFunctionPointer("coreclr_shutdown");

        if (netFuncs.init == NULL || netFuncs.createDelegate == NULL || netFuncs.shutdown == NULL) {
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

        logMsg("exePath: %s", exePath);

        const char* propertyKeys[] = {
            "TRUSTED_PLATFORM_ASSEMBLIES"
        };

        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

        std::string tpaList = "";

        for (const auto& dirEntry : recursive_directory_iterator(std::filesystem::current_path() / "NetAssemblies")) {
            if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".dll") {
                tpaList += (dirEntry.path()).string();
#if defined(_WIN32)
                tpaList += ";";
#elif defined(__linux__)
                tpaList += ":";
#endif
            }
        }

        const char* propertyValues[] = {
            tpaList.c_str()
        };

        csharpEditor = editor;
        csharpMainCamera = interfaces.mainCamera;

        int ret = netFuncs.init(exePath, "WorldsEngine", 1, propertyKeys, propertyValues, &hostHandle, &domainId);

        if (ret < 0) {
            logErr("Failed to initialise coreclr: 0x%x", ret);
            return false;
        }

        // C# bools are always 4 bytes
        uint32_t(*initFunc)(entt::registry* reg);
        createManagedDelegate("WorldsEngine.WorldsEngine", "Init", (void**)&initFunc);

        if (!initFunc(&reg))
            return false;

        createManagedDelegate("WorldsEngine.WorldsEngine", "Update", (void**)&updateFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "Simulate", (void**)&simulateFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "OnSceneStart", (void**)&sceneStartFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "EditorUpdate", (void**)&editorUpdateFunc);
        createManagedDelegate("WorldsEngine.Registry", "OnNativeEntityDestroy", (void**)&nativeEntityDestroyFunc);
        createManagedDelegate("WorldsEngine.Registry", "SerializeManagedComponents", (void**)&serializeComponentsFunc);
        createManagedDelegate("WorldsEngine.Registry", "DeserializeManagedComponent", (void**)&deserializeComponentFunc);
        createManagedDelegate("WorldsEngine.Physics", "HandleCollisionFromNative", (void**)&physicsContactFunc);
        createManagedDelegate("WorldsEngine.Registry", "CopyManagedComponents", (void**)&copyManagedComponentsFunc);


        reg.on_destroy<Transform>().connect<&DotNetScriptEngine::onTransformDestroy>(*this);

        JsonSceneSerializer::setScriptEngine(this);

        return true;
    }

    void DotNetScriptEngine::onTransformDestroy(entt::registry& reg, entt::entity ent) {
        nativeEntityDestroyFunc((uint32_t)ent);
    }

    void DotNetScriptEngine::onSceneStart() {
        ZoneScoped;

        sceneStartFunc();
    }

    void DotNetScriptEngine::onUpdate(float deltaTime) {
        ZoneScoped;

        updateFunc(deltaTime);
    }

    void DotNetScriptEngine::onEditorUpdate(float deltaTime) {
        ZoneScoped;

        editorUpdateFunc(deltaTime);
    }

    void DotNetScriptEngine::onSimulate(float deltaTime) {
        ZoneScoped;

        simulateFunc(deltaTime);
    }

    void DotNetScriptEngine::handleCollision(entt::entity entity, PhysicsContactInfo* contactInfo) {
        physicsContactFunc((uint32_t)entity, contactInfo);
    }

    void DotNetScriptEngine::serializeManagedComponents(nlohmann::json& entityJson, entt::entity entity) {
        serializeComponentsFunc((void*)&entityJson, (uint32_t)entity);
    }

    void DotNetScriptEngine::deserializeManagedComponent(const char* id, const nlohmann::json& componentJson, entt::entity entity) {
        std::string cJsonStr = componentJson.dump();
        deserializeComponentFunc(id, cJsonStr.c_str(), (uint32_t)entity);
    }

    void DotNetScriptEngine::copyManagedComponents(entt::entity from, entt::entity to) {
        copyManagedComponentsFunc(from, to);
    }

    void DotNetScriptEngine::createManagedDelegate(const char* typeName, const char* methodName, void** func) {
        int ret = netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", typeName, methodName, func);
        if (ret < 0) {
            std::string str = "Failed to create delegate ";
            str += typeName;
            str += ".";
            str += methodName;
            fatalErr(str.c_str());
        }
    }
}
