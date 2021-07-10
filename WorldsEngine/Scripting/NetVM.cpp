#include <coreclrhost.h>
#include "Core/Fatal.hpp"
#include "NetVM.hpp"
#include <string>
#include <filesystem>
#include "ImGui/imgui.h"
#include "Core/Log.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define WIN32_LEANER
#define NOMINMAX
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
        return GetProcAddress(libHandle, functionName);
#elif defined(__linux__)
        return dlsym(libHandle, functionName);
#endif
    }

    DotNetScriptEngine::DotNetScriptEngine(entt::registry& reg, EngineInterfaces interfaces)
        : interfaces(interfaces) {
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

        setupBindings();

        // C# bools are always 1 byte
        char(*initFunc)();
        createManagedDelegate("WorldsEngine.WorldsEngine", "Init", (void**)&initFunc);

        if (!initFunc())
            return false;

        createManagedDelegate("WorldsEngine.WorldsEngine", "Update", (void**)&updateFunc);
        createManagedDelegate("WorldsEngine.WorldsEngine", "OnSceneStart", (void**)&sceneStartFunc);

        return true;
    }

    void DotNetScriptEngine::onSceneStart() {
        sceneStartFunc();
    }

    void DotNetScriptEngine::onUpdate(float deltaTime) {
        updateFunc(deltaTime);
    }

    void DotNetScriptEngine::onSimulate(float deltaTime) {
    }

    void DotNetScriptEngine::fireEvent(entt::entity scriptEnt, const char* evt) {
    }

    struct ImGuiFunctionPointers {
        bool (*begin)(const char* name);
        void (*text)(const char* text);
        bool (*button)(const char* text, float sizeX, float sizeY);
        void (*sameLine)(float offsetFromStartX, float spacing);
        void (*end)();
    };

    enum CSMessageSeverity {
        CS_Verbose,
        CS_Info,
        CS_Warning,
        CS_Error
    };

    struct LoggingFuncPtrs {
        void (*log)(CSMessageSeverity severity, const char* text);
    };

    bool imgui_begin(const char* name) {
        return ImGui::Begin(name);
    }

    void imgui_text(const char* text) {
        ImGui::TextUnformatted(text);
    }

    bool imgui_button(const char* text, float sizeX, float sizeY) {
        return ImGui::Button(text, ImVec2(sizeX, sizeY));
    }

    void imgui_sameLine(float offsetFromStartX, float spacing) {
        ImGui::SameLine(offsetFromStartX, spacing);
    }

    void imgui_end() {
        ImGui::End();
    }

    void logging_log(CSMessageSeverity severity, const char* text) {
        switch (severity) {
            case CS_Verbose:
                logVrb(WELogCategoryScripting, "%s", text);
                break;
            case CS_Info:
                logMsg(WELogCategoryScripting, "%s", text);
                break;
            case CS_Warning:
                logWarn(WELogCategoryScripting, "%s", text);
                break;
            case CS_Error:
                logErr(WELogCategoryScripting, "%s", text);
                break;
        }
    }

    void DotNetScriptEngine::createManagedDelegate(const char* typeName, const char* methodName, void** func) {
        int ret = netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", typeName, methodName, func);
        if (ret < 0)
            fatalErr("Failed to create delegate");
    }

    void DotNetScriptEngine::setupBindings() {
        ImGuiFunctionPointers imfp;
        imfp.begin = imgui_begin;
        imfp.text = imgui_text;
        imfp.button = imgui_button;
        imfp.sameLine = imgui_sameLine;
        imfp.end = imgui_end;

        void (*setImguiBindings)(ImGuiFunctionPointers);

        createManagedDelegate("WorldsEngine.ImGui", "SetFunctionPointers", (void**)&setImguiBindings);
        setImguiBindings(imfp);

        LoggingFuncPtrs lfp;
        lfp.log = logging_log;
        void (*setLogBindings)(LoggingFuncPtrs);
        createManagedDelegate("WorldsEngine.Logger", "SetFunctionPointers", (void**)&setLogBindings);
        setLogBindings(lfp);
    }
}
