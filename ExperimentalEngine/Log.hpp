#pragma once
#include <SDL2/SDL_log.h>
#include "LogCategories.hpp"

template<typename... Args> void logErr(const char* fmt, Args... args) {
    SDL_LogError(worlds::WELogCategoryEngine, fmt, args...);
}

template<typename... Args> void logErr(int category, const char* fmt, Args... args) {
    SDL_LogError(category, fmt, args...);
}

template<typename... Args> void logWarn(const char* fmt, Args... args) {
    SDL_LogWarn(worlds::WELogCategoryEngine, fmt, args...);
}

template<typename... Args> void logWarn(int category, const char* fmt, Args... args) {
    SDL_LogWarn(category, fmt, args...);
}

template<typename... Args> void logMsg(const char* fmt, Args... args) {
    SDL_LogMessage(worlds::WELogCategoryEngine, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

template<typename... Args> void logMsg(SDL_LogPriority priority, const char* fmt, Args... args) {
    SDL_LogMessage(worlds::WELogCategoryEngine, priority, fmt, args...);
}

template<typename... Args> void logMsg(int category, const char* fmt, Args... args) {
    SDL_LogMessage(category, SDL_LOG_PRIORITY_INFO, fmt, args...);
}