#include "ConvergeEventHandler.hpp"
#include <iostream>
#include <Core/Engine.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
LONG unhandledExceptionHandler(LPEXCEPTION_POINTERS exceptionPtrs) {
    FILE* f = fopen("crash.txt", "w");
    fprintf(f, "hey, we're still in alpha ok?\n");
    auto record = exceptionPtrs->ExceptionRecord;
    fprintf(f, "exception code: %u\n", record->ExceptionCode);
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
    SetUnhandledExceptionFilter(unhandledExceptionHandler);
#endif
    worlds::EngineInitOptions initOptions;

    std::vector<char*> startupCommands;

    bool ds = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--editor") == 0) {
            initOptions.runAsEditor = true;
        }

        if (strcmp(argv[i], "--vr") == 0) {
            initOptions.enableVR = true;
        }

        if (strcmp(argv[i], "--dedicated-server") == 0) {
           ds = true; 
           initOptions.dedicatedServer = true;
        }

        if (argv[i][0] == '+') {
            startupCommands.push_back(argv[i] + 1);
        }
    }

    if (ds && (initOptions.enableVR || initOptions.runAsEditor)) {
        std::cerr << argv[0] << ": invalid arguments.\n";
        return -1;
    }

    initOptions.useEventThread = true;

    converge::EventHandler evtHandler {ds};
    initOptions.eventHandler = &evtHandler;

    worlds::WorldsEngine engine(initOptions, argv[0]);

    for (auto& cmd : startupCommands) {
        worlds::g_console->executeCommandStr(cmd);
    }
    
    engine.mainLoop();

    return 0;
}
