#include "Fatal.hpp"
#include <SDL2/SDL_messagebox.h>

void fatalErr(const char* msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Worlds Engine", msg, nullptr);
    abort();
}