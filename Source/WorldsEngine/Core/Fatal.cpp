#include "Fatal.hpp"
#include "Log.hpp"
#include <SDL_messagebox.h>
#include <string>

template <typename... Args> const char* format(const char* fmt, Args... args)
{
    int size = snprintf(nullptr, 0, fmt, args...) + 1;
    char* buf = (char*)std::malloc(size);

    snprintf(buf, size, fmt, args...);
    return buf;
}

namespace worlds
{
    void fatalErrInternal(const char *msg, const char *file, int line)
    {
        const char *formatted = format("%s\n(file: %s, line %i)", msg, file, line);
        logErr("%s", formatted);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Worlds Engine", formatted, nullptr);
        abort();
    }

    void assertInternal(const char* condition, const char* file, int line)
    {
        const char* formatted = format("Assertion failed!\n%s\nFile %s, line %i", condition, file, line);
        logErr("%s", formatted);
        SDL_MessageBoxButtonData buttonData[] = {
            { 0, 0, "Abort" },
            { 0, 1, "Ignore" }
        };
        SDL_MessageBoxData mbData{};
        mbData.title = "Assertion Failed";
        mbData.message = formatted;
        mbData.flags = SDL_MESSAGEBOX_ERROR;
        mbData.buttons = buttonData;
        mbData.numbuttons = 2;
        int buttonId;
        SDL_ShowMessageBox(&mbData, &buttonId);

        if (buttonId == 0)
        {
            logErr("Aborting...");
            abort();
        }
        else
        {
            logWarn("Assertion ignored!");
        }
    }
}
