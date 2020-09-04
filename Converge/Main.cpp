#define SDL_MAIN_HANDLED
#include "ConvergeEventHandler.hpp"
#include <Engine.hpp>

int main(int argc, char** argv) {
    worlds::EngineInitOptions initOptions{ false, -1 };

    converge::EventHandler evtHandler;
    initOptions.eventHandler = &evtHandler;

    worlds::initEngine(initOptions, argv[0]);

    return 0;
}