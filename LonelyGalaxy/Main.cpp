#define _CRT_SECURE_NO_WARNINGS
#include "EventHandler.hpp"
#include <Core/Engine.hpp>

namespace lg {
    void registerComponentMeta();
}

// 32 bit systems are not supported!
// Make sure that we're 64 bit
int _dummy[sizeof(void*) - 7];

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
LONG unhandledExceptionHandler(LPEXCEPTION_POINTERS exceptionPtrs) {
    FILE* f = fopen("crash.txt", "w");
    fprintf(f, "hey, we're still in alpha ok?\n");
    fprintf(f, "(disregard if we are no longer in alpha)\n");
    auto record = exceptionPtrs->ExceptionRecord;
    fprintf(f, "exception code: %lu\n", record->ExceptionCode);
    fprintf(f, "address: 0x%zX\n", (uint64_t)(uintptr_t)record->ExceptionAddress);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        fprintf(f, "(btw, the access violation was a ");
        if (record->ExceptionInformation[0] == 1)
            fprintf(f, "write");
        else if (record->ExceptionInformation[1] == 0)
            fprintf(f, "read");
        else
            fprintf(f, "screwup so bad we can't even tell if it's read or write");
        fprintf(f, ")\n");
    }
    fclose(f);

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPtrs;
    exceptionInfo.ClientPointers = false;

    HANDLE dumpFile = CreateFileA("latest_crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpNormal, &exceptionInfo, 0, 0);
    CloseHandle(dumpFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    if (!IsDebuggerPresent())
        SetUnhandledExceptionFilter(unhandledExceptionHandler);
#endif
    worlds::EngineInitOptions initOptions;
    initOptions.gameName = "Lonely Galaxy";
    initOptions.enableVR = true;

    std::vector<std::string> startupCommands;

    bool ds = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--editor") == 0) {
            initOptions.runAsEditor = true;
        }

        if (strcmp(argv[i], "--novr") == 0) {
            initOptions.enableVR = false;
        }

        if (strcmp(argv[i], "--dedicated-server") == 0) {
           ds = true;
           initOptions.dedicatedServer = true;
        }

        if (argv[i][0] == '+') {
            std::string strArg = argv[i];
            size_t colonPos = strArg.find(":");
            if (colonPos != std::string::npos) {
                std::string cmd = strArg.substr(1, colonPos - 1);
                std::string cmdArg = strArg.substr(colonPos + 1);
                startupCommands.push_back(cmd + " " + cmdArg);
            } else {
                startupCommands.push_back(argv[i + 1]);
            }
        }
    }

    if (ds && (initOptions.enableVR || initOptions.runAsEditor)) {
        fprintf(stderr, "%s: invalid arguments.\n", argv[0]);
        return -1;
    }

    initOptions.useEventThread = false;

    lg::EventHandler evtHandler {ds};
    initOptions.eventHandler = &evtHandler;

    worlds::WorldsEngine engine(initOptions, argv[0]);

    lg::registerComponentMeta();

    for (auto& cmd : startupCommands) {
        worlds::g_console->executeCommandStr(cmd);
        logMsg("Executed startup command \"%s\"", cmd.c_str());
    }

    engine.mainLoop();

    return 0;
}
