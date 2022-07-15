#include <Core/IGameEventHandler.hpp>
#include <SDL_timer.h>
#include <core.h>

#include <Core/Engine.hpp>
#include <Core/IGameEventHandler.hpp>
#include <Core/Log.hpp>
#define DISCORD_RPC

namespace worlds
{
    discord::Core* discordCore;

    SDL_TimerID presenceUpdateTimer;

    WorldsEngine* engine;

#ifdef DISCORD_RPC
    void onDiscordReady(discord::User& user)
    {
        logMsg("Rich presence ready for %s", user.GetUsername());

        presenceUpdateTimer = SDL_AddTimer(
            1000,
            [](uint32_t interval, void*) {
                std::string state =
                    "what"; //((engine->runAsEditor ? "Editing " : "On ") + engine->getCurrentSceneInfo().name);
#ifndef NDEBUG
                state += "(DEVELOPMENT BUILD)";
#endif
                discord::Activity activity{};
                activity.SetState(state.c_str());
                activity.SetType(discord::ActivityType::Playing);
                auto& actAssets = activity.GetAssets();
                actAssets.SetLargeImage("logo");
                actAssets.SetLargeText("Private");

                if (!engine->runAsEditor)
                {
                    auto& party = activity.GetParty();
                    party.SetId("1365");
                    party.GetSize().SetMaxSize(256);
                    party.GetSize().SetCurrentSize(1);
                    activity.GetSecrets().SetJoin("someone");
                }

                discordCore->ActivityManager().UpdateActivity(activity, [](discord::Result res) {
                    if (res != discord::Result::Ok)
                    {
                        logWarn("failed to update activity");
                    }
                });
                return interval;
            },
            nullptr);
    }
#endif

    void initRichPresence(EngineInterfaces interfaces)
    {
#ifdef DISCORD_RPC
        engine = interfaces.engine;
        return;

        auto result = discord::Core::Create(742075252028211310, DiscordCreateFlags_Default, &discordCore);

        if (result != discord::Result::Ok)
        {
            logErr("failed to initialise discord :(");
            return;
        }

        discordCore->SetLogHook(discord::LogLevel::Debug, [](discord::LogLevel level, const char* msg) {
            SDL_LogPriority priority = SDL_LOG_PRIORITY_DEBUG;
            switch (level)
            {
            case discord::LogLevel::Debug:
                priority = SDL_LOG_PRIORITY_DEBUG;
                break;
            case discord::LogLevel::Error:
                priority = SDL_LOG_PRIORITY_ERROR;
                break;
            case discord::LogLevel::Info:
                priority = SDL_LOG_PRIORITY_INFO;
                break;
            case discord::LogLevel::Warn:
                priority = SDL_LOG_PRIORITY_WARN;
                break;
            }
            SDL_LogMessage(WELogCategoryEngine, priority, "%s", msg);
        });

        discordCore->UserManager().OnCurrentUserUpdate.Connect([]() {
            discord::User currentUser;
            auto result = discordCore->UserManager().GetCurrentUser(&currentUser);
            if (result != discord::Result::Ok)
            {
                logErr("failed to get current discord user :(");
                return;
            }
        });
#endif
    }

    void tickRichPresence()
    {
#ifdef DISCORD_RPC
        if (discordCore)
            discordCore->RunCallbacks();
#endif
    }

    void shutdownRichPresence()
    {
#ifdef DISCORD_RPC
        delete discordCore;
#endif
    }
}
