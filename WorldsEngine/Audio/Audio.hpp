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

struct stb_vorbis;

namespace worlds {
    enum class MixerChannel : uint32_t {
        Music,
        SFX,
        Count
    };

    struct AudioSource {
        AudioSource(AssetID clipId) :
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

    struct AudioTrigger {
        bool playOnce = false;
        bool hasPlayed = false;
        bool resetOnEntry = false;
    };

    struct ReverbProbeBox {
        glm::vec3 bounds;
    };

    struct AudioListenerOverride {};

    class AudioSystem {
    public:
        AudioSystem();
        void initialise(entt::registry& worldState);
        void loadAudioScene(std::string sceneName);
        void update(entt::registry& worldState, glm::vec3 listenerPos, glm::quat listenerRot);
        void setPauseState(bool paused);
        void cancelOneShots();
        inline bool getPauseState() { return isPaused; }
        void shutdown(entt::registry& worldState);
        void resetPlaybackPositions();
        void playOneShotClip(AssetID id, glm::vec3 location, bool spatialise = false, float volume = 1.0f, MixerChannel channel = MixerChannel::SFX);
        void precacheAudioClip(AssetID clipId);
        void setChannelVolume(MixerChannel channel, float volume) { mixerVolumes[static_cast<int>(channel)] = volume; }
        float getChannelVolume(MixerChannel channel) { return mixerVolumes[static_cast<int>(channel)]; }
        static AudioSystem* getInstance() { return instance; }
    private:
        static AudioSystem* instance;

        enum class ClipType {
            Vorbis,
            MP3
        };

        struct LoadedClip {
            AssetID id;
            ClipType type;
            int channels;
            int sampleRate;
            int sampleCount;
            float* data;
            uint32_t refCount;
        };

        struct SpatialVoiceInfo {
            float distance;
            glm::vec3 direction;
            IPLDirectSoundPath soundPath;
        };

        struct PhononVoiceEffects {
            IPLhandle binauralEffect;
            IPLhandle directSoundEffect;
        };

        struct Voice {
            bool lock : 1;
            bool isPlaying : 1;
            bool loop : 1;
            bool spatialise : 1;
            LoadedClip* clip;
            float volume;
            int playbackPosition;
            SpatialVoiceInfo spatialInfo;
            PhononVoiceEffects iplFx;
            MixerChannel channel;
        };

        struct PlayingOneshot {
            glm::vec3 location;
            uint32_t voiceIdx;
        };

        struct AudioSourceInternal {
            uint32_t voiceIdx;
            bool lastPlaying;
        };

        static void audioCallback(void* userData, uint8_t* streamU8, int len);
        static void cmdSetMixerVolume(void* obj, const char* params);

        uint32_t allocateVoice();
        void decodeVorbis(stb_vorbis* vorb, AudioSystem::LoadedClip& clip);
        void onAudioSourceConstruct(entt::registry& reg, entt::entity ent);
        void onAudioSourceDestroy(entt::registry& reg, entt::entity ent);
        LoadedClip& loadAudioClip(AssetID id);

        SDL_AudioDeviceID devId;
        float cpuUsage;
        float volume;
        std::unordered_map<entt::entity, AudioSourceInternal> internalAs;
        std::unordered_map<AssetID, LoadedClip> loadedClips;
        slib::HeapArray<Voice> voices;
        slib::StaticAllocList<PlayingOneshot> oneshots;
        int channelCount;
        int numSamples;
        int sampleRate;
        bool isPaused;
        glm::vec3 listenerPosition;
        glm::quat listenerRotation;
        ConVar showDebugMenuVar;

        IPLhandle sceneHandle;
        bool audioSceneLoaded;

        IPLhandle phononContext = nullptr;
        IPLhandle binauralRenderer = nullptr;
        IPLhandle environment = nullptr;

        float mixerVolumes[static_cast<int>(MixerChannel::Count)];
        static void mixVoice(Voice& voice, int numMonoSamplesNeeded, float* stream, AudioSystem* _this);
        LoadedClip missingClip;
    };
}
