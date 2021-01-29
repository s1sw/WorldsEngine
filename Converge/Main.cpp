#include "ConvergeEventHandler.hpp"
#include <iostream>
#include <Engine.hpp>

int main(int argc, char** argv) {
    worlds::EngineInitOptions initOptions;

    std::vector<char*> startupCommands;

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

        if (argv[i][0] == '+') {
            startupCommands.push_back(argv[i] + 1);
        }
    }

    if (ds && (initOptions.enableVR || initOptions.runAsEditor)) {
        std::cerr << argv[0] << ": invalid arguments.\n";
        return -1;
    }

    initOptions.useEventThread = true;

    converge::EventHandler evtHandler {ds};
    initOptions.eventHandler = &evtHandler;

    worlds::WorldsEngine engine(initOptions, argv[0]);

    for (auto& cmd : startupCommands) {
        worlds::g_console->executeCommandStr(cmd);
    }
    
    engine.mainLoop();

    return 0;
}
