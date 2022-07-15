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

void fatalErrInternal(const char* msg, const char* file, int line)
{
    const char* formatted = format("%s\n(file: %s, line %i)", msg, file, line);
    logErr("%s", formatted);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Worlds Engine", formatted, nullptr);
    abort();
}
