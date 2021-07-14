#include <coreclrhost.h>
#include "Core/Fatal.hpp"
#include "NetVM.hpp"
#include <string>
#include <filesystem>
#include "ImGui/imgui.h"
#include "Core/Log.hpp"
#include "Core/Transform.hpp"
#include "Core/NameComponent.hpp"
#include <entt/entity/registry.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define WIN32_LEANER
#include <Windows.h>
typedef HMODULE LibraryHandle_t;
#define NET_LIBRARY_PATH "./NetAssemblies/coreclr.dll"
#elif defined(__linux__)
#include <limits.h>
typedef void* LibraryHandle_t;
#include <unistd.h>
#include <dlfcn.h>
#define NET_LIBRARY_PATH "NetAssemblies/libcoreclr.so"
#endif

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility ("default")))
#endif

enum CSMessageSeverity {
    CS_Verbose,
    CS_Info,
    CS_Warning,
    CS_Error
};

using namespace worlds;
extern "C" {
    EXPORT bool imgui_begin(const char* name) {
        return ImGui::Begin(name);
    }

    EXPORT void imgui_text(const char* text) {
        ImGui::TextUnformatted(text);
    }

    EXPORT bool imgui_button(const char* text, float sizeX, float sizeY) {
        return ImGui::Button(text, ImVec2(sizeX, sizeY));
    }

    EXPORT void imgui_sameLine(float offsetFromStartX, float spacing) {
        ImGui::SameLine(offsetFromStartX, spacing);
    }

    EXPORT void imgui_end() {
        ImGui::End();
    }

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

    EXPORT void registry_getTransform(entt::registry* registry, uint32_t entity, Transform* output) {
        entt::entity enttEntity = (entt::entity)entity;
        *output = registry->get<Transform>(enttEntity);
    }

    EXPORT void registry_setTransform(entt::registry* registry, uint32_t entity, Transform* output) {
        entt::entity enttEntity = (entt::entity)entity;
        registry->get<Transform>(enttEntity) = *output;
    }

    EXPORT void registry_eachTransform(entt::registry* registry, void (*callback)(uint32_t)) {
        registry->each([&](entt::entity ent) {
            callback((uint32_t)ent);
        });
    }

    EXPORT uint32_t registry_getEntityNameLength(entt::registry* registry, uint32_t entityId) {
        entt::entity enttEntity = (entt::entity)entityId;
        if (!registry->has<NameComponent>(enttEntity)) { return ~0u; }
        NameComponent& nc = registry->get<NameComponent>((entt::entity)entityId);
        return nc.name.size();
    }

    EXPORT void registry_getEntityName(entt::registry* registry, uint32_t entityId, char* buffer) {
        entt::entity enttEntity = (entt::entity)entityId;
        if (!registry->has<NameComponent>(enttEntity)) {
            return;
        }
        NameComponent& nc = registry->get<NameComponent>(enttEntity);
        buffer[nc.name.size()] = 0;
        strncpy(buffer, nc.name.c_str(), nc.name.size());
    }
}

namespace worlds {
    LibraryHandle_t loadLibrary(const char* path) {
#if defined(_WIN32)
        return LoadLibraryExA(path, NULL, 0);
#elif defined(__linux__)
        return dlopen(path, RTLD_NOW);
#endif
    }

    void* getLibraryFunction(LibraryHandle_t libHandle, const char* functionName) {
#if defined(_WIN32)
        return (void*)GetProcAddress(libHandle, functionName);
#elif defined(__linux__)
        return dlsym(libHandle, functionName);
#endif
    }

    DotNetScriptEngine::DotNetScriptEngine(entt::registry& reg, EngineInterfaces interfaces)
        : interfaces(interfaces)
        , reg(reg) {
    }

    bool DotNetScriptEngine::initialise() {
        LibraryHandle_t netLibrary = loadLibrary(NET_LIBRARY_PATH);

        if (netLibrary == 0) {
            logErr("Failed to load coreclr");
            return false;
        }

        netFuncs.init = (coreclr_initialize_ptr)getLibraryFunction(netLibrary, "coreclr_initialize");
        netFuncs.createDelegate = (coreclr_create_delegate_ptr)getLibraryFunction(netLibrary, "coreclr_create_delegate");
        netFuncs.shutdown = (coreclr_shutdown_ptr)getLibraryFunction(netLibrary, "coreclr_shutdown");

        if (netFuncs.init == NULL || netFuncs.createDelegate == NULL || netFuncs.shutdown == NULL) {
            logErr("Failed to get .net functions");
            return false;
        }

#if defined(_WIN32)
        char exePath[MAX_PATH];
        GetModuleFileName(0, exePath, MAX_PATH);
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

        int ret = netFuncs.init(exePath, "WorldsEngine", 1, propertyKeys, propertyValues, &hostHandle, &domainId);

        if (ret < 0) {
            logErr("Failed to initialise coreclr: 0x%x", ret);
            return false;
        }

        // C# bools are always 1 byte
        char(*initFunc)();
        createManagedDelegate("WorldsEngine.WorldsEngine", "Init", (void**)&initFunc);

        if (!initFunc())
            return false;

        createManagedDelegate("WorldsEngine.WorldsEngine", "Update", (void**)&updateFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "OnSceneStart", (void**)&sceneStartFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "EditorUpdate", (void**)&editorUpdateFunc);

        return true;
    }

    void DotNetScriptEngine::onSceneStart() {
        sceneStartFunc(&reg);
    }

    void DotNetScriptEngine::onUpdate(float deltaTime) {
        updateFunc(deltaTime);
    }

    void DotNetScriptEngine::onEditorUpdate(float deltaTime) {
        editorUpdateFunc(deltaTime);
    }

    void DotNetScriptEngine::onSimulate(float deltaTime) {
    }

    void DotNetScriptEngine::fireEvent(entt::entity scriptEnt, const char* evt) {
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
