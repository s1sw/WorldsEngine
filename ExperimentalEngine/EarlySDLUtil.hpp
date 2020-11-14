#pragma once
#include <SDL2/SDL_render.h>
#include <string>

namespace worlds {
    SDL_Surface* loadDataFileToSurface(std::string fName);
    void setWindowIcon(SDL_Window* win, const char* iconName = "icon.png");
}