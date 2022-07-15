#pragma once
#include <SDL_log.h>

namespace worlds
{
    enum LogCategory
    {
        WELogCategoryEngine = SDL_LOG_CATEGORY_CUSTOM,
        WELogCategoryAudio,
        WELogCategoryRender,
        WELogCategoryUI,
        WELogCategoryApp,
        WELogCategoryScripting,
        WELogCategoryPhysics
    };
}

#if defined(__clang__) || defined(__GNUC__)
#define PRINTF_FMT(idx) __attribute__((__format__(__printf__, idx, 0)))
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
#else
#define PRINTF_FMT(idx)
#endif

template <typename... Args> PRINTF_FMT(1) void logErr(const char* fmt, Args... args)
{
    SDL_LogError(worlds::WELogCategoryEngine, fmt, args...);
}

template <typename... Args> PRINTF_FMT(2) void logErr(int category, const char* fmt, Args... args)
{
    SDL_LogError(category, fmt, args...);
}

template <typename... Args> PRINTF_FMT(1) void logWarn(const char* fmt, Args... args)
{
    SDL_LogWarn(worlds::WELogCategoryEngine, fmt, args...);
}

template <typename... Args> PRINTF_FMT(2) void logWarn(int category, const char* fmt, Args... args)
{
    SDL_LogWarn(category, fmt, args...);
}

template <typename... Args> PRINTF_FMT(1) void logMsg(const char* fmt, Args... args)
{
    SDL_LogMessage(worlds::WELogCategoryEngine, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

template <typename... Args> PRINTF_FMT(2) void logMsg(SDL_LogPriority priority, const char* fmt, Args... args)
{
    SDL_LogMessage(worlds::WELogCategoryEngine, priority, fmt, args...);
}

template <typename... Args> PRINTF_FMT(2) void logMsg(int category, const char* fmt, Args... args)
{
    SDL_LogMessage(category, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

template <typename... Args> PRINTF_FMT(2) void logVrb(int category, const char* fmt, Args... args)
{
    SDL_LogMessage(category, SDL_LOG_PRIORITY_VERBOSE, fmt, args...);
}

template <typename... Args> PRINTF_FMT(1) void logVrb(const char* fmt, Args... args)
{
    SDL_LogMessage(worlds::WELogCategoryEngine, SDL_LOG_PRIORITY_VERBOSE, fmt, args...);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
