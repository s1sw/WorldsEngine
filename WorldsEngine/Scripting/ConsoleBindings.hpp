#include "Core/Console.hpp"
#include "Export.hpp"

using namespace worlds;

extern "C"
{
    EXPORT void console_registerCommand(void (*funcPtr)(void*, const char*), char* name, char* help, void* obj)
    {
        g_console->registerCommand(funcPtr, strdup(name), help == nullptr ? nullptr : strdup(help), obj);
    }
}
