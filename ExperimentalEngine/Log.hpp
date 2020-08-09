#pragma once
#include <SDL2/SDL_log.h>

template<typename... Args> void logErr(const char* fmt, Args... args) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, args...);
}

template<typename... Args> void logMsg(const char* fmt, Args... args) {
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args...);
}