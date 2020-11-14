#include "ConvergeEventHandler.hpp"
#include <Engine.hpp>
#define _AMD64_
#include <minwindef.h>

int main(int argc, char** argv) {
    worlds::EngineInitOptions initOptions;

    converge::EventHandler evtHandler;
    initOptions.eventHandler = &evtHandler;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--editor") == 0) {
            initOptions.runAsEditor = true;
        }

        if (strcmp(argv[i], "--vr") == 0) {
            initOptions.enableVR = true;
        }
    }

    worlds::WorldsEngine engine(initOptions, argv[0]);
    engine.mainLoop();

    return 0;
}
