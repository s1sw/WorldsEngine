#include <discord_rpc.h>
#include "Log.hpp"
#include <SDL2/SDL_timer.h>
#include "Engine.hpp"

namespace worlds {
    extern bool runAsEditor;
    extern SceneInfo currentScene;

    SDL_TimerID presenceUpdateTimer;

    void onDiscordReady(const DiscordUser* user) {
        logMsg("Rich presence ready for %s", user->username);

        presenceUpdateTimer = SDL_AddTimer(1000, [](uint32_t interval, void*) {
            std::string state = ((runAsEditor ? "Editing " : "On ") + currentScene.name);
#ifndef NDEBUG
            state += "(DEVELOPMENT BUILD)";
#endif
            DiscordRichPresence richPresence;
            memset(&richPresence, 0, sizeof(richPresence));
            richPresence.state = state.c_str();
            richPresence.largeImageKey = "logo";
            richPresence.largeImageText = "Private";
            if (!runAsEditor) {
                richPresence.partyId = "1365";
                richPresence.partyMax = 256;
                richPresence.partySize = 1;
                richPresence.joinSecret = "someone";
            }

            Discord_UpdatePresence(&richPresence);
            return interval;
            }, nullptr);
    }

    void initRichPresence() {
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        handlers.ready = onDiscordReady;
        Discord_Initialize("742075252028211310", &handlers, 0, nullptr);
    }

    void tickRichPresence() {
        Discord_RunCallbacks();
    }

    void shutdownRichPresence() {
        Discord_Shutdown();
    }
}