#pragma once
#include <SDL2/SDL_log.h>

namespace worlds {
    enum LogCategory {
        WELogCategoryEngine = SDL_LOG_CATEGORY_CUSTOM,
        WELogCategoryAudio,
        WELogCategoryRender,
        WELogCategoryUI,
        WELogCategoryApp
    };
}