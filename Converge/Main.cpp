#include "ConvergeEventHandler.hpp"
#include <Engine.hpp>

int main(int argc, char** argv) {
    worlds::EngineInitOptions initOptions;

    bool ds = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--editor") == 0) {
            initOptions.runAsEditor = true;
        }

        if (strcmp(argv[i], "--vr") == 0) {
            initOptions.enableVR = true;
        }

        if (strcmp(argv[i], "--dedicated-server") == 0) {
           ds = true; 
           initOptions.dedicatedServer = true;
        }
    }

    converge::EventHandler evtHandler {ds};
    initOptions.eventHandler = &evtHandler;

    worlds::WorldsEngine engine(initOptions, argv[0]);
    engine.mainLoop();

    return 0;
}
