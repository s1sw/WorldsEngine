#include <SDL_audio.h>
#include "../Core/Log.hpp"
#include "Audio.hpp"
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include "../Util/TimingUtil.hpp"
#include "../IO/IOUtil.hpp"
#include "../ImGui/imgui.h"
#include "../Core/Transform.hpp"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include "phonon.h"

namespace worlds {
    int playbackSamples;

    double dspTime = 0.0;

    double squarify(double in) {
        return in > 0.0 ? 1.0 : -1.0;
    }

    double sineWave(double time, double freq) {
        return sin(time * 2.0 * glm::pi<double>() * freq);
    }

    double remapToZeroOne(double in) {
        return (in + 1.0) * 0.5;
    }

    // Converts a MIDI note to a frequency in Hz
    float pitch(float p) {
        return powf(1.059460646483f, p - 69.0f) * 440.0f;
    }

    float sine(float t, float freq) {
        return (sinf(t * freq * 6.283184f) + 1.0f) * 0.5f;
    }

    static float timeAccumulator = 0.0f;

    AudioSystem::AudioSystem()
        : showDebugMenuVar("a_showDebugMenu", "0") {
        for (int i = 0; i < static_cast<int>(MixerChannel::Count); i++)
            mixerVolumes[i] = 1.0f;
        setChannelVolume(MixerChannel::Music, 0.0f);
        g_console->registerCommand(cmdSetMixerVolume, "a_setMixerVol", "Sets the volume of the specified mixer track. Run like this: a_setMixerVol <track id> <volume>", this);
    }

    template <typename T>
    inline void mixClip(AudioSystem::LoadedClip& clip, T& sourceInfo, int numMonoSamplesNeeded, int numSamplesNeeded, float* stream, AudioSystem* _this) {
        int samplesRemaining = clip.sampleCount - sourceInfo.playbackPosition;

        float vol = _this->mixerVolumes[static_cast<int>(sourceInfo.channel)] * sourceInfo.volume;

        if (clip.channels == 2) {
            for (int i = 0; i < std::min(numMonoSamplesNeeded, samplesRemaining); i++) {
                int outPos = i * 2;
                int inPos = (i + sourceInfo.playbackPosition) * 2;
                stream[outPos] += clip.data[inPos] * vol;
                stream[outPos + 1] += clip.data[inPos + 1] * vol;
            }
        } else if (!sourceInfo.spatialise) {
            for (int i = 0; i < std::min(numMonoSamplesNeeded, samplesRemaining); i++) {
                int outPos = i * 2;
                int inPos = i + sourceInfo.playbackPosition;
                stream[outPos] += clip.data[inPos] * vol;
                stream[outPos + 1] += clip.data[inPos] * vol;
            }
        }

        if (sourceInfo.spatialise && clip.channels == 1) {
            IPLAudioFormat clipFormat;
            clipFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            clipFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
            clipFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

            IPLAudioBuffer inBuffer{
                clipFormat, std::min(numMonoSamplesNeeded, samplesRemaining), &clip.data[sourceInfo.playbackPosition]
            };

            void* outTemp = std::malloc(numMonoSamplesNeeded * 2 * sizeof(float));
            memset(outTemp, 0, numMonoSamplesNeeded * 2 * sizeof(float));

            IPLAudioFormat outFormat;
            outFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            outFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
            outFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

            IPLAudioBuffer outBuffer{ outFormat, numMonoSamplesNeeded, (float*)outTemp };

            iplApplyBinauralEffect(
                    _this->binauralEffect, 
                    _this->binauralRenderer, 
                    inBuffer, IPLVector3{ sourceInfo.direction.x, sourceInfo.direction.y, sourceInfo.direction.z }, 
                    IPL_HRTFINTERPOLATION_BILINEAR, 1.0f, outBuffer);

            for (int i = 0; i < numSamplesNeeded; i++) {
                stream[i] += ((float*)outTemp)[i] * vol;
            }

            std::free(outTemp);
        }

        sourceInfo.playbackPosition += numMonoSamplesNeeded;

        if (sourceInfo.playbackPosition >= clip.sampleCount) {
            if (sourceInfo.loop) {
                sourceInfo.playbackPosition = 0;
            } else {
                sourceInfo.finished = true;
                sourceInfo.isPlaying = false;
            }
        }
    }

    void AudioSystem::audioCallback(void* userData, uint8_t* streamU8, int len) {
        float* stream = reinterpret_cast<float*>(streamU8);
        int streamLen = len / sizeof(float);
        // The audio callback has to be static, so store "this" in the userData
        AudioSystem* _this = reinterpret_cast<AudioSystem*>(userData);

        PerfTimer timer;

        // Calculate the length of the buffer in seconds
        // Buffer Length / number of channels / number of samples / 4 (size of float in bytes)
        float secondBufferLength = (float)len / (float)_this->channelCount / 44100.f / (float)sizeof(float);

        memset(stream, 0, len);

        int numSamplesNeeded = streamLen;
        int numMonoSamplesNeeded = streamLen / 2;

        for (auto& p : _this->internalAs) {
            if (!p.second.isPlaying)
                continue;

            LoadedClip& playedClip = _this->loadedClips.at(p.second.clipId);

            mixClip(playedClip, p.second, numMonoSamplesNeeded, numSamplesNeeded, stream, _this);
        }

        for (auto& c : _this->oneShotClips) {
            LoadedClip& playedClip = *c.clip;

            mixClip(playedClip, c, numMonoSamplesNeeded, numSamplesNeeded, stream, _this);
        }

        for (int i = 0; i < streamLen; i++) {
            stream[i] *= _this->volume;
        }

        timeAccumulator += secondBufferLength;
        auto ms = timer.stopGetMs();

        _this->cpuUsage = (ms / (secondBufferLength * 1000.0));
    }

    void AudioSystem::cmdSetMixerVolume(void* obj, const char* params) {
        AudioSystem* _this = reinterpret_cast<AudioSystem*>(obj);
        std::string paramStr(params);

        size_t spacePos = paramStr.find(' ');
        if (spacePos == std::string::npos)
            return;

        int trackID = std::stoi(paramStr.substr(0, spacePos));
        float volume = std::stof(paramStr.substr(spacePos));

        if (trackID >= static_cast<int>(MixerChannel::Count))
            return;

        _this->mixerVolumes[trackID] = volume;
    }

    void phLog(const char* msg) {
        logMsg(WELogCategoryAudio, "Phonon: %s", msg);
    }

    AudioSystem* AudioSystem::instance;

    void AudioSystem::initialise(entt::registry& reg) {
        instance = this;
        missingClip = loadAudioClip(g_assetDB.addOrGetExisting("Audio/SFX/dlgsound.ogg"));
        //audioSceneLoaded = false;
        volume = 1.0f;
        logMsg(WELogCategoryAudio, "Initialising audio system");
        SDL_AudioSpec want, have;
        memset(&want, 0, sizeof(want));
        want.freq = 44100;
        want.format = AUDIO_F32;
        want.channels = 2;
        want.samples = 4096;
        want.callback = &AudioSystem::audioCallback;
        want.userdata = this;
        devId = SDL_OpenAudioDevice(nullptr, false, &want, &have, 0);

        if (devId == 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device");
        } else {
            SDL_PauseAudioDevice(devId, 0);
        }

        logMsg(WELogCategoryAudio, "Opened audio device at %ihz with %i channels and %i samples", have.freq, have.channels, have.samples);
        channelCount = have.channels;

        reg.on_construct<AudioSource>().connect<&AudioSystem::onAudioSourceConstruct>(*this);
        reg.on_destroy<AudioSource>().connect<&AudioSystem::onAudioSourceDestroy>(*this);

        SDL_Log("Initialising Phonon");
        iplCreateContext((IPLLogFunction)phLog, nullptr, nullptr, &phononContext);

        IPLRenderingSettings settings{ have.freq, have.samples };

        IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, nullptr };
        iplCreateBinauralRenderer(phononContext, settings, hrtfParams, &binauralRenderer);

        IPLAudioFormat audioIn;
        audioIn.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioIn.channelLayout = IPL_CHANNELLAYOUT_MONO;
        audioIn.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        IPLAudioFormat audioOut;
        audioOut.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioOut.channelLayout = IPL_CHANNELLAYOUT_STEREO;
        audioOut.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        iplCreateBinauralEffect(binauralRenderer, audioIn, audioOut, &binauralEffect);
    }

    void AudioSystem::loadAudioScene(std::string sceneName) {
        auto loadFileRes = LoadFileToString("audioScenes/" + sceneName + ".dat");

        if (loadFileRes.error != IOError::None) {
            logErr(WELogCategoryAudio, "Failed to load audio scene %s (%s)", sceneName.c_str(), getIOErrorStr(loadFileRes.error));
            return;
        }
    }

    void AudioSystem::update(entt::registry& reg, glm::vec3 listenerPos, glm::quat listenerRot) {
        if (showDebugMenuVar.getInt()) {
            if (ImGui::Begin("Audio Testing")) {
                ImColor col = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
                if (cpuUsage > 0.5f) {
                    col = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
                }
                ImGui::TextColored(col, "CPU Usage: %.2f%%", cpuUsage * 100.0f);
                ImGui::Text("Playing Clip Count: %zu", reg.view<AudioSource>().size());
                ImGui::Text("Playing one shot count: %zu", oneShotClips.size());
                ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

                for (auto& p : internalAs) {
                    ImGui::Separator();
                    ImGui::Text("Playback position: %i", p.second.playbackPosition);
                    ImGui::Text("Is Playing: %i", p.second.isPlaying);
                    ImGui::Text("Volume: %f", p.second.volume);
                }
            }

            ImGui::End();
        }

        SDL_LockAudioDevice(devId);

        reg.view<Transform, AudioSource>().each([this, listenerPos, listenerRot](auto ent, auto& transform, auto& audioSource) {
            AudioSourceInternal& asi = internalAs.at(ent);
            asi.volume = audioSource.volume;
            asi.clipId = audioSource.clipId;
            asi.direction = glm::normalize(listenerPos - transform.position) * listenerRot;

            if (asi.finished) {
                audioSource.isPlaying = false;
                asi.finished = false;
            }

            asi.isPlaying = audioSource.isPlaying;
            asi.loop = audioSource.loop;
            asi.spatialise = audioSource.spatialise;
            asi.channel = audioSource.channel;
            });

        if (oneShotClips.size())
            oneShotClips.erase(std::remove_if(oneShotClips.begin(), oneShotClips.end(), [](OneShotClipInfo& clipInfo) { return clipInfo.finished; }), oneShotClips.end());

        for (auto& c : oneShotClips) {
            c.direction = glm::normalize(c.location - listenerPos) * listenerRot;
        }

        SDL_UnlockAudioDevice(devId);
    }

    void AudioSystem::setPauseState(bool paused) {
        isPaused = paused;
        SDL_PauseAudioDevice(devId, paused);
    }

    void AudioSystem::shutdown(entt::registry& reg) {
        reg.on_construct<AudioSource>().disconnect<&AudioSystem::onAudioSourceConstruct>(*this);
        reg.on_destroy<AudioSource>().disconnect<&AudioSystem::onAudioSourceDestroy>(*this);
        SDL_CloseAudioDevice(devId);
    }

    void AudioSystem::resetPlaybackPositions() {
        for (auto& p : internalAs) {
            p.second.playbackPosition = 0;
        }
    }

    void AudioSystem::playOneShotClip(AssetID id, glm::vec3 location, bool spatialise, float volume, MixerChannel channel) {
        if (loadedClips.count(id) == 0)
            loadAudioClip(id);
        oneShotClips.push_back(OneShotClipInfo{ &loadedClips.at(id), 0, location, glm::vec3(0.0f), spatialise, volume, false, false, false, channel });
    }

    void AudioSystem::onAudioSourceConstruct(entt::registry& reg, entt::entity ent) {
        SDL_LockAudioDevice(devId);

        AudioSource& as = reg.get<AudioSource>(ent);

        AudioSourceInternal asi;
        loadAudioClip(as.clipId).refCount++;
        asi.isPlaying = as.isPlaying;
        asi.clipId = as.clipId;
        asi.finished = false;
        asi.playbackPosition = 0;

        internalAs.insert({ ent, asi });
        SDL_UnlockAudioDevice(devId);
    }

    void AudioSystem::onAudioSourceDestroy(entt::registry& reg, entt::entity ent) {
        SDL_LockAudioDevice(devId);

        LoadedClip& lc = loadedClips.at(internalAs.at(ent).clipId);
        lc.refCount--;

        if (lc.refCount <= 0) {
            loadedClips.erase(lc.id);
        }

        internalAs.erase(ent);

        SDL_UnlockAudioDevice(devId);
    }

    AudioSystem::LoadedClip& AudioSystem::loadAudioClip(AssetID id) {
        if (loadedClips.count(id) == 1)
            return loadedClips.at(id);
        // find the path
        std::string path = g_assetDB.getAssetPath(id);

        int64_t fLen;
        Result<void*, IOError> res = LoadFileToBuffer(path, &fLen);

        if (res.error != IOError::None) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load audio clip %s", path.c_str());
            return missingClip;
        }

        LoadedClip clip;

        // Decode
        short* stbOut;
        clip.sampleCount = stb_vorbis_decode_memory((const unsigned char*)res.value, fLen, &clip.channels, &clip.sampleRate, &stbOut);
        std::free(res.value);

        if (clip.sampleCount == -1) {
            logErr(WELogCategoryAudio, "Could not decode audio clip %s", path.c_str());
            return missingClip;
        }

        logMsg(WELogCategoryAudio, "Loaded %i samples across %i channels with a sample rate of %i", clip.sampleCount, clip.channels, clip.sampleRate);

        if (clip.sampleRate != 44100)
            logWarn(WELogCategoryAudio, "Clip %s does not have a sample rate of 44100hz (%i). It will not play back correctly.", path.c_str(), clip.sampleRate);

        clip.data = (float*)std::malloc(sizeof(float) * clip.sampleCount * clip.channels);

        for (int i = 0; i < clip.sampleCount * clip.channels; i++) {
            clip.data[i] = ((float)stbOut[i]) / 32768.0f;
        }

        std::free(stbOut);

        clip.id = id;

        loadedClips.insert({ id, clip });

        return loadedClips.at(id);
    }
}
