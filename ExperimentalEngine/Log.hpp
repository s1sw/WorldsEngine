#pragma once
#include <SDL2/SDL_log.h>
#include "LogCategories.hpp"

#define PRINTF_FMT(idx) __attribute__((__format__ (__printf__, idx, 0)))
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"

template<typename... Args> 
PRINTF_FMT(1)
void logErr(const char* fmt, Args... args) {
    SDL_LogError(worlds::WELogCategoryEngine, fmt, args...);
}

template<typename... Args> 
PRINTF_FMT(2)
void logErr(int category, const char* fmt, Args... args) {
    SDL_LogError(category, fmt, args...);
}

template<typename... Args>
PRINTF_FMT(1)
void logWarn(const char* fmt, Args... args) {
    SDL_LogWarn(worlds::WELogCategoryEngine, fmt, args...);
}

template<typename... Args> 
PRINTF_FMT(2)
void logWarn(int category, const char* fmt, Args... args) {
    SDL_LogWarn(category, fmt, args...);
}

template<typename... Args> 
PRINTF_FMT(1)
void logMsg(const char* fmt, Args... args) {
    SDL_LogMessage(worlds::WELogCategoryEngine, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

template<typename... Args> 
PRINTF_FMT(2)
void logMsg(SDL_LogPriority priority, const char* fmt, Args... args) {
    SDL_LogMessage(worlds::WELogCategoryEngine, priority, fmt, args...);
}

template<typename... Args> 
PRINTF_FMT(2)
void logMsg(int category, const char* fmt, Args... args) {
    SDL_LogMessage(category, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

#pragma clang diagnostic pop
