#include <coreclrhost.h>
#include "NetVM.hpp"
#include <string>
#include <filesystem>
#include "ImGui/imgui.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define WIN32_LEANER
#define NOMINMAX
#include <Windows.h>
typedef HMODULE LibraryHandle_t;
#elif defined(__linux__)
#include <limits.h>
typedef void* LibraryHandle_t;
#endif

namespace worlds {
    LibraryHandle_t loadLibrary(const char* path) {
#if defined(_WIN32)
        return LoadLibraryExA(path, NULL, 0);
#elif defined(__linux__)
        return dlopen(path, 0);
#endif
    }

    void* getLibraryFunction(LibraryHandle_t libHandle, const char* functionName) {
#if defined(_WIN32)
        return GetProcAddress(libHandle, functionName);
#endif
    }

    DotNetScriptEngine::DotNetScriptEngine(entt::registry& reg, EngineInterfaces interfaces)
        : interfaces(interfaces) {
    }

    bool DotNetScriptEngine::initialise() {
        LibraryHandle_t netLibrary = loadLibrary("./NetAssemblies/coreclr.dll");

        if (netLibrary == 0)
            return false;

        netFuncs.init = (coreclr_initialize_ptr)getLibraryFunction(netLibrary, "coreclr_initialize");
        netFuncs.createDelegate = (coreclr_create_delegate_ptr)getLibraryFunction(netLibrary, "coreclr_create_delegate");
        netFuncs.shutdown = (coreclr_shutdown_ptr)getLibraryFunction(netLibrary, "coreclr_shutdown");

        if (netFuncs.init == NULL || netFuncs.createDelegate == NULL || netFuncs.shutdown == NULL)
            return false;

#if defined(_WIN32)
        char exePath[MAX_PATH];
        GetModuleFileName(0, exePath, MAX_PATH);
#elif defined(__linux__)
        char exePath[PATH_MAX];
        ssize_t readChars = readlink("/proc/self/exe", exePath, PATH_MAX);
        exePath[readChars] = 0;
#endif

        const char* propertyKeys[] = {
            "TRUSTED_PLATFORM_ASSEMBLIES"
        };

        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

        std::string tpaList = "";

        for (const auto& dirEntry : recursive_directory_iterator(std::filesystem::current_path() / "netAssemblies")) {
            if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".dll") {
                tpaList += (dirEntry.path()).string();
                tpaList += ";";
            }
        }

        const char* propertyValues[] = {
            tpaList.c_str()
        };

        int ret = netFuncs.init(exePath, "WorldsEngine", 1, propertyKeys, propertyValues, &hostHandle, &domainId);

        if (ret < 0) {
            return false;
        }

        setupBindings();

        // C# bools are always 1 byte
        char(*initFunc)();
        ret = netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", "WorldsEngine.WorldsEngine", "Init", (void**)&initFunc);

        if (ret < 0)
            return false;

        if (!initFunc())
            return false;

        netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", "WorldsEngine.WorldsEngine", "Update", (void**)&updateFunc);

        return true;
    }

    void DotNetScriptEngine::onSceneStart() {
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

    void DotNetScriptEngine::setupBindings() {
        ImGuiFunctionPointers imfp;
        imfp.begin = imgui_begin;
        imfp.text = imgui_text;
        imfp.button = imgui_button;
        imfp.sameLine = imgui_sameLine;
        imfp.end = imgui_end;

        void (*setImguiBindings)(ImGuiFunctionPointers);

        netFuncs.createDelegate(hostHandle, domainId, "WorldsEngineManaged", "WorldsEngine.ImGui", "SetFunctionPointers", (void**)&setImguiBindings);
        setImguiBindings(imfp);
    }
}
