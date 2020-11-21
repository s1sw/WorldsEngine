#include <discord_rpc.h>
#include "Log.hpp"
#include <SDL2/SDL_timer.h>
#include "Engine.hpp"

namespace worlds {
    extern SceneInfo currentScene;

    SDL_TimerID presenceUpdateTimer;

    WorldsEngine* engine;

#ifdef DISCORD_RPC
    void onDiscordReady(const DiscordUser* user) {
        logMsg("Rich presence ready for %s", user->username);

        presenceUpdateTimer = SDL_AddTimer(1000, [](uint32_t interval, void*) {
            std::string state = ((engine->runAsEditor ? "Editing " : "On ") + currentScene.name);
#ifndef NDEBUG
            state += "(DEVELOPMENT BUILD)";
#endif
            DiscordRichPresence richPresence;
            memset(&richPresence, 0, sizeof(richPresence));
            richPresence.state = state.c_str();
            richPresence.largeImageKey = "logo";
            richPresence.largeImageText = "Private";
            if (!engine->runAsEditor) {
                richPresence.partyId = "1365";
                richPresence.partyMax = 256;
                richPresence.partySize = 1;
                richPresence.joinSecret = "someone";
            }

            Discord_UpdatePresence(&richPresence);
            return interval;
            }, nullptr);
    }
#endif

    void initRichPresence(EngineInterfaces interfaces) {
#ifdef DISCORD_RPC
        engine = interfaces.engine;
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        handlers.ready = onDiscordReady;
        Discord_Initialize("742075252028211310", &handlers, 0, nullptr);
#endif
    }

    void tickRichPresence() {
#ifdef DISCORD_RPC
        Discord_RunCallbacks();
#endif
    }

    void shutdownRichPresence() {
#ifdef DISCORD_RPC
        Discord_Shutdown();
#endif
    }
}
