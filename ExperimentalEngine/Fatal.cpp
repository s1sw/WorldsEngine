#include "Fatal.hpp"
#include "Log.hpp"
#include <SDL2/SDL_messagebox.h>

void fatalErr(const char* msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Worlds Engine", msg, nullptr);
    logErr(SDL_LOG_PRIORITY_CRITICAL, "%s", msg);
    abort();
}
