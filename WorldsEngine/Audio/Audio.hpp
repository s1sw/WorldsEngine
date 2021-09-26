#pragma once
#include <unordered_map>
#include "../Core/AssetDB.hpp"
#include <entt/entt.hpp>
#include "../Core/Console.hpp"
#include <slib/StaticAllocList.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SDL_audio.h>
#include <phonon.h>
#include <slib/HeapArray.hpp>
#include <glm/glm.hpp>
#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <robin_hood.h>

struct stb_vorbis;

namespace worlds {
    enum class MixerChannel : uint32_t {
        Music,
        SFX,
        Count
    };

    struct OldAudioSource {
        OldAudioSource(AssetID clipId) :
            clipId(clipId),
            volume(1.0f),
            isPlaying(false),
            playOnSceneOpen(true),
            loop(false),
            spatialise(true),
            channel(MixerChannel::SFX) {
        }

        AssetID clipId;
        float volume;
        bool isPlaying;
        bool playOnSceneOpen;
        bool loop;
        bool spatialise;
        MixerChannel channel;
    };

    struct AudioSource {
        FMOD::Studio::EventInstance* eventInstance = nullptr;
        bool playOnSceneStart = true;

        const std::string_view eventPath() { return _eventPath; }
        void changeEventPath(const std::string_view& eventPath);
        FMOD_STUDIO_PLAYBACK_STATE playbackState();
    private:
        std::string _eventPath;
    };

    struct AudioTrigger {
        bool playOnce = false;
        bool hasPlayed = false;
        bool resetOnEntry = false;
    };

    struct ReverbProbeBox {
        glm::vec3 bounds;
    };

    struct AudioListenerOverride { int meh; };

    class AudioSystem {
    public:
        AudioSystem();
        void initialise(entt::registry& worldState);
        void loadMasterBanks();
        void update(entt::registry& worldState, glm::vec3 listenerPos, glm::quat listenerRot, float deltaTime);
        void stopEverything(entt::registry& reg);
        void playOneShotClip(AssetID id, glm::vec3 location, bool spatialise = false, float volume = 1.0f, MixerChannel channel = MixerChannel::SFX);
        void playOneShotEvent(const char* eventPath, glm::vec3 location, float volume = 1.0f);
        inline bool getPauseState() { return false; }
        void shutdown(entt::registry& worldState);
        void precacheAudioClip(AssetID id) {}
        static AudioSystem* getInstance() { return instance; }
        FMOD::Studio::Bank* loadBank(const char* path);
    private:
        void onAudioSourceDestroy(entt::registry& reg, entt::entity ent);

        friend struct AudioSource;

        glm::vec3 lastListenerPos;
        static AudioSystem* instance;
        FMOD::Studio::System* studioSystem;
        FMOD::System* system;
        FMOD::Studio::Bank* masterBank;
        FMOD::Studio::Bank* stringsBank;

        uint32_t phononPluginHandle;
        IPLContext phononContext;
        IPLHRTF phononHrtf;
        IPLSimulator simulator;

        robin_hood::unordered_map<const char*, FMOD::Studio::Bank*> loadedBanks;
        robin_hood::unordered_map<AssetID, FMOD::Sound*> sounds;
    };
}
