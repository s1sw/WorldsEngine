#pragma once
#include "../Core/AssetDB.hpp"
#include <entt/entt.hpp>
#include "../Core/Console.hpp"
#include <glm/gtc/quaternion.hpp>
#include "phonon.h"
#include <glm/glm.hpp>

struct stb_vorbis;

namespace worlds {
    enum class MixerChannel {
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
        inline bool getPauseState() { return isPaused; }
        void shutdown(entt::registry& worldState);
        void resetPlaybackPositions();
        void playOneShotClip(AssetID id, glm::vec3 location, bool spatialise = false, float volume = 1.0f, MixerChannel channel = MixerChannel::SFX);
        inline void setChannelVolume(MixerChannel channel, float volume) { mixerVolumes[static_cast<int>(channel)] = volume; }
        inline float getChannelVolume(MixerChannel channel) { return mixerVolumes[static_cast<int>(channel)]; }
        static AudioSystem* getInstance() { return instance; }
    private:
        static AudioSystem* instance;
        struct AudioSourceInternal {
            AssetID clipId;
            float volume;
            int playbackPosition;
            bool isPlaying;
            bool finished;
            bool loop;
            bool spatialise;
            glm::vec3 direction;
            float distance;
            MixerChannel channel;
            IPLDirectSoundPath soundPath;
            IPLhandle binauralEffect;
            IPLhandle directSoundEffect;
        };

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

        struct OneShotClipInfo {
            LoadedClip* clip;
            int playbackPosition;
            glm::vec3 location;
            glm::vec3 direction;
            bool spatialise;
            float volume;
            bool isPlaying;
            bool finished;
            bool loop;
            float distance;
            MixerChannel channel;
            IPLDirectSoundPath soundPath;
            IPLhandle binauralEffect;
            IPLhandle directSoundEffect;
        };

        static void audioCallback(void* userData, uint8_t* streamU8, int len);
        static void cmdSetMixerVolume(void* obj, const char* params);

        void decodeVorbis(stb_vorbis* vorb, AudioSystem::LoadedClip& clip);
        void onAudioSourceConstruct(entt::registry& reg, entt::entity ent);
        void onAudioSourceDestroy(entt::registry& reg, entt::entity ent);
        LoadedClip& loadAudioClip(AssetID id);

        SDL_AudioDeviceID devId;
        float cpuUsage;
        float volume;
        std::unordered_map<entt::entity, AudioSourceInternal> internalAs;
        std::unordered_map<AssetID, LoadedClip> loadedClips;
        std::vector<OneShotClipInfo> oneShotClips;
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
        template <typename T>
        friend void mixClip(AudioSystem::LoadedClip& clip, T& sourceInfo, int numMonoSamplesNeeded, float* stream, AudioSystem* _this);
        LoadedClip missingClip;
    };
}
