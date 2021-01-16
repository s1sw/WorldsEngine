#include <core.h>
#include "Log.hpp"
#include <SDL2/SDL_timer.h>
#include "Engine.hpp"

namespace worlds {
    extern SceneInfo currentScene;
    discord::Core* discordCore;

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
            discord::Activity activity;
            activity.SetState(state.c_str());
            activity.SetType(discord::ActivityType::Playing);
            auto& actAssets = activity.GetAssets();
            actAssets.SetLargeImage("logo");
            actAssets.SetLargeText("Private");

            if (!engine->runAsEditor) {
                auto& party = activity.GetParty();
                party.SetId("1365");
                party.GetSize().SetMaxSize(256);
                party.GetSize().SetCurrentSize(1);
                activity.GetSecrets().SetJoin("someone");
            }

            discordCore->ActivityManager().UpdateActivity(activity, [](discord::Result res) {
                if (res != discord::Result::Ok) {
                    logWarn("failed to update activity");
                }
            });
            return interval;
            }, nullptr);
    }
#endif

    void initRichPresence(EngineInterfaces interfaces) {
        return;
#ifdef DISCORD_RPC
        engine = interfaces.engine;

        auto result = discord::Core::Create(742075252028211310, DiscordCreateFlags_NoRequireDiscord, &discordCore);

        if (result != discord::Result::Ok) {
            logErr("failed to initialise discord :(");
        }
#endif
    }

    void tickRichPresence() {
#ifdef DISCORD_RPC
        if (discordCore)
            discordCore->RunCallbacks();
#endif
    }

    void shutdownRichPresence() {
#ifdef DISCORD_RPC
        delete discordCore;
#endif
    }
}
