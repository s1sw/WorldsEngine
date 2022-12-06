#pragma once
#include <Core/IGameEventHandler.hpp>

namespace worlds
{
    class IntegratedMenubar
    {
      public:
        IntegratedMenubar(EngineInterfaces interfaces);
        void draw();

      private:
        EngineInterfaces interfaces;
    };
}