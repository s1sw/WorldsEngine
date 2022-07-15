#include "EarlySDLUtil.hpp"
#include "stb_image.h"
#include <SDL_filesystem.h>

namespace worlds
{
    SDL_Surface *loadDataFileToSurface(std::string fName)
    {
        int width, height, channels;

        std::string basePath = SDL_GetBasePath();
        basePath += "EngineData";
#ifdef _WIN32
        basePath += '\\';
#else
        basePath += '/';
#endif
        basePath += fName;
        unsigned char *imgDat = stbi_load(basePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        int shift = 0;
        rmask = 0xff000000 >> shift;
        gmask = 0x00ff0000 >> shift;
        bmask = 0x0000ff00 >> shift;
        amask = 0x000000ff;
#else // little endian, like x86
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
#endif
        return SDL_CreateRGBSurfaceFrom((void *)imgDat, width, height, 32, 4 * width, rmask, gmask, bmask, amask);
    }

    void setWindowIcon(SDL_Window *win, const char *iconName)
    {
        auto surf = loadDataFileToSurface(iconName);
        SDL_SetWindowIcon(win, surf);
        SDL_FreeSurface(surf);
    }
}
