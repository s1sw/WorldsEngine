#pragma once
#include <Render/Camera.hpp>

namespace worlds
{
    struct PickParams
    {
        Camera* cam;
        int screenWidth;
        int screenHeight;
        int pickX;
        int pickY;
    };
}