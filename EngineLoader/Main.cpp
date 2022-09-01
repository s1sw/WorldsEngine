#define _CRT_SECURE_NO_WARNINGS
#include <Core/Engine.hpp>

using namespace worlds;

// 32 bit systems are not supported!
// Make sure that we're 64 bit
int _dummy[sizeof(void *) - 7];

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
LONG unhandledExceptionHandler(LPEXCEPTION_POINTERS exceptionPtrs)
{
    FILE *f = fopen("crash.txt", "w");
    fprintf(f, "hey, we're still in alpha ok?\n");
    fprintf(f, "(disregard if we are no longer in alpha)\n");
    auto record = exceptionPtrs->ExceptionRecord;
    fprintf(f, "exception code: %lu\n", record->ExceptionCode);
    fprintf(f, "address: 0x%zX\n", (uint64_t)(uintptr_t)record->ExceptionAddress);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
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

    HANDLE dumpFile =
        CreateFileA("latest_crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpNormal, &exceptionInfo, 0, 0);
    CloseHandle(dumpFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
    if (!IsDebuggerPresent())
        SetUnhandledExceptionFilter(unhandledExceptionHandler);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    worlds::EngineInitOptions initOptions;
    initOptions.gameName = "Lightline";

    EngineArguments::parseArguments(argc, argv);
    initOptions.runAsEditor = EngineArguments::hasArgument("editor");
    initOptions.enableVR = !EngineArguments::hasArgument("novr");
    initOptions.dedicatedServer = EngineArguments::hasArgument("dedicated-server");

    if (initOptions.dedicatedServer && (initOptions.enableVR || initOptions.runAsEditor))
    {
        fprintf(stderr, "%s: invalid arguments.\n", argv[0]);
        return -1;
    }

    initOptions.useEventThread = false;

    worlds::WorldsEngine engine(initOptions, argv[0]);
    engine.run();

    return 0;
}
