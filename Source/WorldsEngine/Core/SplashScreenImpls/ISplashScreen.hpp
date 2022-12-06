#pragma once

namespace worlds
{
    class ISplashScreen
    {
      public:
        virtual void changeOverlay(const char*) = 0;
        virtual ~ISplashScreen()
        {
        }
    };
}
