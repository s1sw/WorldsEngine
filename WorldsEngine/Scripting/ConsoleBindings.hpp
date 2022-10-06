#include "Core/Console.hpp"
#include "Export.hpp"

using namespace worlds;

extern "C"
{
    EXPORT void console_registerCommand(void (*funcPtr)(int, const char*), char* name, char* help, int commandId)
    {
        g_console->registerCommand(
            [=](const char* args)
            {
               funcPtr(commandId, args);
            }, strdup(name), help == nullptr ? nullptr : strdup(help));
    }

    EXPORT void console_executeCommand(const char* cmd)
    {
        g_console->executeCommandStr(cmd);
    }
}
