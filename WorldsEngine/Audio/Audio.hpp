#pragma once
#include <Core/AssetDB.hpp>
#include <Core/Console.hpp>
#include <SDL_audio.h>
#include <entt/entt.hpp>
#include <glm/gtc/quaternion.hpp>
#include <queue>
#include <slib/StaticAllocList.hpp>
#include <unordered_map>
#ifdef ENABLE_STEAM_AUDIO
#include <phonon.h>
#endif
#include <fmod.hpp>
#include <fmod_studio.hpp>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <slib/HeapArray.hpp>

struct stb_vorbis;

#ifndef ENABLE_STEAM_AUDIO
#define DECLARE_OPAQUE_HANDLE(x) typedef struct _##x##_t* x
DECLARE_OPAQUE_HANDLE(IPLSource);
DECLARE_OPAQUE_HANDLE(IPLContext);
DECLARE_OPAQUE_HANDLE(IPLHRTF);
DECLARE_OPAQUE_HANDLE(IPLScene);
DECLARE_OPAQUE_HANDLE(IPLSimulator);
#undef DECLARE_OPAQUE_HANDLE
#endif

namespace worlds
{
    enum class MixerChannel : uint32_t
    {
        Music,
        SFX,
        Count
    };

    struct OldAudioSource
    {
        OldAudioSource(AssetID clipId)
            : clipId(clipId), volume(1.0f), isPlaying(false), playOnSceneOpen(true), loop(false), spatialise(true),
              channel(MixerChannel::SFX)
        {
        }

        AssetID clipId;
        float volume;
        bool isPlaying;
        bool playOnSceneOpen;
        bool loop;
        bool spatialise;
        MixerChannel channel;
    };

    struct AudioSource
    {
        FMOD::Studio::EventInstance* eventInstance = nullptr;
        bool playOnSceneStart = true;
        IPLSource phononSource = nullptr;

        const std::string_view eventPath()
        {
            return _eventPath;
        }
        void changeEventPath(const std::string_view& eventPath);
        FMOD_STUDIO_PLAYBACK_STATE playbackState();

      private:
        std::string _eventPath;
        bool inPhononSim = false;
        friend class AudioSystem;
    };

    struct AudioTrigger
    {
        bool playOnce = false;
        bool hasPlayed = false;
        bool resetOnEntry = false;
    };

    struct ReverbProbeBox
    {
    };

    struct AudioListenerOverride
    {
        int meh;
    };

    class AudioSystem
    {
      public:
        AudioSystem();
        void initialise(entt::registry& worldState);
        void loadMasterBanks();
        void update(entt::registry& worldState, glm::vec3 listenerPos, glm::quat listenerRot, float deltaTime);
        void stopEverything(entt::registry& reg);
        void playOneShotClip(AssetID id, glm::vec3 location, bool spatialise = false, float volume = 1.0f,
                             MixerChannel channel = MixerChannel::SFX);
        void playOneShotEvent(const char* eventPath, glm::vec3 location, float volume = 1.0f);
        void playOneShotAttachedEvent(const char* eventPath, glm::vec3 location, entt::entity entity,
                                      float volume = 1.0f);
        inline bool getPauseState()
        {
            return false;
        }
        void shutdown(entt::registry& worldState);
        static AudioSystem* getInstance()
        {
            return instance;
        }
        FMOD::Studio::Bank* loadBank(const char* path);
        void bakeProbes(entt::registry& registry);
        void saveAudioScene(entt::registry& reg, const char* path);
        void updateAudioScene(entt::registry& reg);

      private:
        class SteamAudioSimThread;
        void updateSteamAudio(entt::registry& registry, float deltaTime, glm::vec3 listenerPos, glm::quat listenerRot);
        void onAudioSourceDestroy(entt::registry& reg, entt::entity ent);
        static FMOD_RESULT phononEventInstanceCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type,
                                                       FMOD_STUDIO_EVENTINSTANCE* event, void* param);
        IPLScene createScene(entt::registry& reg);
        IPLScene loadScene(const char* path);

        struct AttachedOneshot
        {
            FMOD::Studio::EventInstance* instance;
            FMOD::DSP* phononDsp;
            IPLSource phononSource = nullptr;
            entt::entity entity;
            glm::vec3 lastPosition;
            bool markForRemoval = false;
            float timeSinceStop = 0.0f;
        };

        friend struct AudioSource;

        glm::vec3 lastListenerPos;
        bool available = true;
        bool needsSimCommit = false;
        static AudioSystem* instance;
        FMOD::Studio::System* studioSystem;
        FMOD::System* system;
        FMOD::Studio::Bank* masterBank;
        FMOD::Studio::Bank* stringsBank;
        FMOD::Studio::VCA* masterVCA;

        uint32_t phononPluginHandle;
        IPLContext phononContext;
        IPLHRTF phononHrtf;
        IPLSimulator simulator;
        IPLSource listenerCentricSource;
        IPLScene scene = nullptr;
        float timeSinceLastSim = 0.0f;
        SteamAudioSimThread* simThread;

        std::queue<IPLSource> sourcesToAdd;
        std::queue<IPLSource> sourcesToRemove;
        robin_hood::unordered_map<const char*, FMOD::Studio::Bank*> loadedBanks;
        robin_hood::unordered_map<AssetID, FMOD::Sound*> sounds;
        std::vector<AttachedOneshot*> attachedOneshots;
    };
}
