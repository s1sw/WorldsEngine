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
#include "../Core/Fatal.hpp"
#include <slib/StaticAllocList.hpp>

namespace worlds {
    int playbackSamples;

    double dspTime = 0.0;

    AudioSystem::AudioSystem()
        : showDebugMenuVar("a_showDebugMenu", "0") {
        for (int i = 0; i < static_cast<int>(MixerChannel::Count); i++)
            mixerVolumes[i] = 1.0f;
        setChannelVolume(MixerChannel::Music, 0.0f);
        g_console->registerCommand(cmdSetMixerVolume,
            "a_setMixerVol",
            "Sets the volume of the specified mixer track. Run like this: a_setMixerVol <track id> <volume>",
            this
        );

        g_console->registerCommand(
            [&](void*, const char* arg) { volume = std::atof(arg); },
            "a_setVolume",
            "Sets the volume.",
            nullptr
        );
    }

    float* lastBuffer = nullptr;
    float* tempBuffer = nullptr;
    bool copyBuffer = false;

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
                clipFormat, numMonoSamplesNeeded,
                &clip.data[sourceInfo.playbackPosition], nullptr
            };

            IPLAudioFormat outFormat;
            outFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            outFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
            outFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

            IPLAudioBuffer outBuffer{
                outFormat, std::min(numMonoSamplesNeeded, samplesRemaining), tempBuffer, nullptr };

            glm::vec3 dNorm = glm::normalize(sourceInfo.direction);
            IPLVector3 dir { dNorm.x, dNorm.y, dNorm.z };

            iplApplyBinauralEffect(
                    sourceInfo.binauralEffect,
                    _this->binauralRenderer,
                    inBuffer, dir,
                    IPL_HRTFINTERPOLATION_BILINEAR, 1.0f, outBuffer);

            //float adjDistance = glm::max(sourceInfo.distance + 1.0f, 1.0f);

            //float distFalloff = 1.0f / (adjDistance * adjDistance);

            //for (int i = 0; i < std::min(numMonoSamplesNeeded, samplesRemaining); i++) {
            //    stream[i * 2 + 0] += ((float*)inBuffer.interleavedBuffer)[i] * vol * sourceInfo.soundPath.distanceAttenuation;
            //    stream[i * 2 + 1] += ((float*)inBuffer.interleavedBuffer)[i] * vol * sourceInfo.soundPath.distanceAttenuation;
            //}

            for (int i = 0; i < std::min(numMonoSamplesNeeded, samplesRemaining); i++) {
                stream[i * 2 + 0] += ((float*)tempBuffer)[i * 2 + 0] * vol * sourceInfo.soundPath.distanceAttenuation;
                stream[i * 2 + 1] += ((float*)tempBuffer)[i * 2 + 1] * vol * sourceInfo.soundPath.distanceAttenuation;
            }
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
        float secondBufferLength = (float)len / (float)_this->channelCount / (float)_this->sampleRate / (float)sizeof(float);

        memset(stream, 0, len);

        int numSamplesNeeded = streamLen;
        int numMonoSamplesNeeded = streamLen / 2;

        for (auto& p : _this->internalAs) {
            if (!p.second.isPlaying)
                continue;

            auto clipIt = _this->loadedClips.find(p.second.clipId);

            if (clipIt == _this->loadedClips.end()) {
                continue;
            }

            LoadedClip& playedClip = clipIt->second;

            mixClip(playedClip, p.second, numMonoSamplesNeeded, numSamplesNeeded, stream, _this);
        }

        for (auto& c : _this->oneShotClips) {
            LoadedClip& playedClip = *c.clip;

            mixClip(playedClip, c, numMonoSamplesNeeded, numSamplesNeeded, stream, _this);
        }

        for (int i = 0; i < streamLen; i++) {
            stream[i] *= _this->volume;
        }

        if (lastBuffer && copyBuffer)
            memcpy(lastBuffer, stream, len);

        dspTime += secondBufferLength;
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

        if (trackID >= static_cast<int>(MixerChannel::Count)) {
            logErr(WELogCategoryAudio, "a_setMixerVolume: invalid channel");
            return;
        }

        _this->mixerVolumes[trackID] = volume;
    }

    void phLog(const char* msg) {
        logMsg(WELogCategoryAudio, "Phonon: %s", msg);
    }

    void checkIplError(IPLerror err) {
        if (err != IPL_STATUS_SUCCESS) {
            fatalErr("IPL fail");
        }
    }

    AudioSystem* AudioSystem::instance;

    void AudioSystem::initialise(entt::registry& reg) {
        instance = this;
        missingClip = loadAudioClip(g_assetDB.addOrGetExisting("Audio/SFX/dlgsound.ogg"));

        volume = 1.0f;
        logMsg(WELogCategoryAudio, "Initialising audio system");

        SDL_AudioSpec want, have;
        memset(&want, 0, sizeof(want));
        want.freq = 44100;
        want.format = AUDIO_F32;
        want.channels = 2;
        want.samples = 1024;
        want.callback = &AudioSystem::audioCallback;
        want.userdata = this;
        devId = SDL_OpenAudioDevice(nullptr, false, &want, &have, 0);

        lastBuffer = (float*)malloc(have.samples * have.channels * sizeof(float));
        tempBuffer = (float*)malloc(have.samples * have.channels * sizeof(float));

        logMsg(WELogCategoryAudio, "Opened audio device at %ihz with %i channels and %i samples", have.freq, have.channels, have.samples);
        channelCount = have.channels;
        numSamples = have.samples;
        sampleRate = have.freq;

        reg.on_construct<AudioSource>().connect<&AudioSystem::onAudioSourceConstruct>(*this);
        reg.on_destroy<AudioSource>().connect<&AudioSystem::onAudioSourceDestroy>(*this);

        SDL_Log("Initialising Phonon");
        checkIplError(iplCreateContext((IPLLogFunction)phLog, nullptr, nullptr, &phononContext));

        IPLRenderingSettings settings{ have.freq, have.samples, IPL_CONVOLUTIONTYPE_PHONON };

        IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, nullptr };
        checkIplError(iplCreateBinauralRenderer(phononContext, settings, hrtfParams, &binauralRenderer));

        IPLAudioFormat audioIn;
        audioIn.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioIn.channelLayout = IPL_CHANNELLAYOUT_MONO;
        audioIn.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        IPLAudioFormat audioOut;
        audioOut.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioOut.channelLayout = IPL_CHANNELLAYOUT_STEREO;
        audioOut.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        checkIplError(iplCreateDirectSoundEffect(audioIn, audioIn, settings, &directSoundEffect));

        IPLSimulationSettings simulationSettings{};
        simulationSettings.sceneType = IPL_SCENETYPE_PHONON;
        simulationSettings.maxNumOcclusionSamples = 32;
        simulationSettings.numRays = 1024;
        simulationSettings.numBounces = 2;
        simulationSettings.numThreads = 8;
        simulationSettings.irDuration = 0.5f;
        simulationSettings.ambisonicsOrder = 0;
        simulationSettings.maxConvolutionSources = 32;
        simulationSettings.bakingBatchSize = 1;
        simulationSettings.irradianceMinDistance = 0.3f;

        checkIplError(iplCreateEnvironment(phononContext, NULL,
                simulationSettings, NULL, NULL, &environment));

        if (devId == 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device");
        } else {
            SDL_PauseAudioDevice(devId, 0);
        }
    }

    void AudioSystem::loadAudioScene(std::string sceneName) {
        auto loadFileRes = LoadFileToString("audioScenes/" + sceneName + ".dat");

        if (loadFileRes.error != IOError::None) {
            logErr(WELogCategoryAudio, "Failed to load audio scene %s (%s)", sceneName.c_str(), getIOErrorStr(loadFileRes.error));
            return;
        }
    }

    IPLVector3 convVec(const glm::vec3& v) {
        return IPLVector3 { v.x, v.y, v.z };
    }

    worlds::ConVar showAudioOscilloscope{ "a_showOscilloscope", "0", "Shows oscilloscope in the audio debug menu." };

    void AudioSystem::update(entt::registry& reg, glm::vec3 listenerPos, glm::quat listenerRot) {
        copyBuffer = showAudioOscilloscope.getInt();
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

                if (copyBuffer) {
                    ImGui::PlotLines("L Audio", [](void* data, int idx) {
                        return ((float*)data)[idx * 2];
                    }, lastBuffer, numSamples /2, 0, nullptr, -1.0f, 1.0f, ImVec2(300, 150));

                    ImGui::PlotLines("R Audio", [](void* data, int idx) {
                        return ((float*)data)[idx * 2 + 1];
                    }, lastBuffer, numSamples /2, 0, nullptr, -1.0f, 1.0f, ImVec2(300, 150));
                }

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

        reg.view<AudioSource, AudioTrigger, Transform>().each(
                [&](auto ent, AudioSource& as, AudioTrigger& at, Transform& t) {
                glm::vec3 ma = t.position + t.scale;
                glm::vec3 mi = t.position - t.scale;

                bool insideTrigger = glm::all(glm::lessThan(listenerPos, ma)) &&
                    glm::all(glm::greaterThan(listenerPos, mi));

                if (insideTrigger) {
                    if (at.playOnce && at.hasPlayed) return;

                    bool justEntered = !as.isPlaying;

                    if (justEntered && at.resetOnEntry) {
                        internalAs.at(ent).playbackPosition = 0;
                    }

                    as.isPlaying = true;
                    at.hasPlayed = true;
                } else if (!at.playOnce) {
                    as.isPlaying = false;
                }
        });

        reg.view<Transform, AudioSource>().each([this, listenerPos, listenerRot](auto ent, auto& transform, auto& audioSource) {
            AudioSourceInternal& asi = internalAs.at(ent);
            asi.volume = audioSource.volume;
            asi.clipId = audioSource.clipId;

            glm::vec3 dirVec = listenerPos - transform.position;
            asi.direction = glm::normalize(dirVec) * listenerRot;
            asi.distance = glm::length(dirVec);

            if (asi.finished) {
                audioSource.isPlaying = false;
                asi.finished = false;
            }

            asi.isPlaying = audioSource.isPlaying;
            asi.loop = audioSource.loop;
            asi.spatialise = audioSource.spatialise;
            asi.channel = audioSource.channel;
        });

        if (oneShotClips.size()) {
            static slib::StaticAllocList<IPLhandle> fxKillList { 512 };
            fxKillList.clear();
            oneShotClips.erase(std::remove_if(oneShotClips.begin(), oneShotClips.end(),
                [](OneShotClipInfo& clipInfo) {
                    if (clipInfo.finished && clipInfo.spatialise) fxKillList.add(clipInfo.binauralEffect);
                    return clipInfo.finished;
                }
            ), oneShotClips.end());

            for (auto& handle : fxKillList) {
                iplDestroyBinauralEffect(&handle);
            }
        }

        for (auto& c : oneShotClips) {
            glm::vec3 dirVec = listenerPos - c.location;
            c.direction = glm::normalize(glm::normalize(dirVec) * listenerRot);
            c.distance = glm::length(dirVec);

            IPLDistanceAttenuationModel distanceAttenuationModel {
                .type = IPL_DISTANCEATTENUATION_DEFAULT
            };

            IPLAirAbsorptionModel airAbsorptionModel {
                .type = IPL_AIRABSORPTION_DEFAULT
            };

            IPLSource src {
                .position = convVec(c.location),
                .ahead = IPLVector3 { 0.0f, 0.0f, 1.0f },
                .up = IPLVector3 { 0.0f, 1.0f, 0.0f },
                .right = IPLVector3 { 1.0f, 0.0f, 0.0f },
                .directivity = IPLDirectivity {
                    .dipoleWeight = 0.0f,
                    .dipolePower = 0.0f,
                    .callback = nullptr
                },
                .distanceAttenuationModel = distanceAttenuationModel,
                .airAbsorptionModel = airAbsorptionModel
            };

            c.soundPath = iplGetDirectSoundPath(environment,
                convVec(listenerPos),
                convVec(listenerRot * glm::vec3 { 0.0f, 0.0f, 1.0f }),
                convVec(listenerRot * glm::vec3 { 0.0f, 1.0f, 0.0f }),
                src,
                0.0f, // not used yet
                0, // also not used yet
                IPL_DIRECTOCCLUSION_NONE,
                IPL_DIRECTOCCLUSION_RAYCAST);
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
        SDL_LockAudioDevice(devId);

        IPLhandle effect;

        IPLAudioFormat audioIn;
        audioIn.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioIn.channelLayout = IPL_CHANNELLAYOUT_MONO;
        audioIn.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        IPLAudioFormat audioOut;
        audioOut.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
        audioOut.channelLayout = IPL_CHANNELLAYOUT_STEREO;
        audioOut.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        if (spatialise)
            checkIplError(iplCreateBinauralEffect(binauralRenderer, audioIn, audioOut, &effect));

        oneShotClips.push_back(OneShotClipInfo{ &loadedClips.at(id), 0, location, glm::vec3(0.0f), spatialise, volume, false, false, false, 0.01f, channel, {}, effect });

        SDL_UnlockAudioDevice(devId);
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

        auto lcIter = loadedClips.find(internalAs.at(ent).clipId);

        if (lcIter != loadedClips.end()) {
            LoadedClip& lc = loadedClips.at(internalAs.at(ent).clipId);
            lc.refCount--;

            if (lc.refCount <= 0) {
                loadedClips.erase(lc.id);
            }
        }

        internalAs.erase(ent);

        SDL_UnlockAudioDevice(devId);
    }

    void interleaveFloats(int len, float** data, float* out) {
        for (int i = 0; i < len; i++) {
            out[i * 2 + 0] = data[0][i];
            out[i * 2 + 1] = data[1][i];
        }
    }

    void AudioSystem::decodeVorbis(stb_vorbis* vorb, AudioSystem::LoadedClip& clip) {
        stb_vorbis_info info = stb_vorbis_get_info(vorb);

        int bufferLen = info.channels * stb_vorbis_stream_length_in_samples(vorb);
        clip.channels = info.channels;
        clip.sampleRate = info.sample_rate;

        float* data = (float*)malloc(bufferLen * sizeof(float));
        memset(data, 0, bufferLen * sizeof(float));

        int offset = 0;
        int totalSamples = 0;
        float** output;

        while (true) {
            // number of samples
            int n = stb_vorbis_get_frame_float(vorb, nullptr, &output);
            if (n == 0) break;
            assert(offset < bufferLen);
            totalSamples += n;

            if (clip.channels == 2)
                interleaveFloats(n, output, data + offset);
            else
                memcpy(data + offset, output[0], n * sizeof(float));

            offset += info.channels * n;
        }

        stb_vorbis_close(vorb);
        assert((totalSamples * info.channels) == bufferLen);
        clip.sampleCount = totalSamples;
        clip.data = data;
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
        int error;
        stb_vorbis* vorb = stb_vorbis_open_memory(
                (const uint8_t*)res.value, fLen, &error, nullptr);

        if (vorb == nullptr) {
            const std::unordered_map<int, const char*> errorStrings = {
                { VORBIS__no_error, "No Error" },
                { VORBIS_need_more_data, "Need more data" },
                { VORBIS_invalid_api_mixing, "Invalid API mixing" },
                { VORBIS_feature_not_supported, "Feature not supported (likely floor 0)" },
                { VORBIS_too_many_channels, "Too many channels" },
                { VORBIS_file_open_failure, "File open failure" },
                { VORBIS_seek_without_length, "Tried to seek without length" },
                { VORBIS_unexpected_eof, "Unexpected EOF" },
                { VORBIS_seek_invalid, "Invalid seek" },
                { VORBIS_invalid_setup, "Invalid setup" },
                { VORBIS_invalid_stream, "Invalid stream" },
                { VORBIS_missing_capture_pattern, "Missing capture pattern" },
                { VORBIS_invalid_stream_structure_version, "Invalid stream structure version" },
                { VORBIS_continued_packet_flag_invalid, "Continued packet flag invalid" },
                { VORBIS_incorrect_stream_serial_number, "Incorrect stream serial number" },
                { VORBIS_invalid_first_page, "Invalid first page" },
                { VORBIS_bad_packet_type, "Bad packet type" },
                { VORBIS_cant_find_last_page, "Can't find last page" },
                { VORBIS_seek_failed, "Seek failed" },
                { VORBIS_ogg_skeleton_not_supported, "Ogg skeleton not supported" }
            };
            logErr(WELogCategoryAudio, "Couldn't decode audio clip %s: error %i (%s)", path.c_str(), error, errorStrings.at(error));
            return missingClip;
        }

        decodeVorbis(vorb, clip);

        std::free(res.value);

        logMsg(WELogCategoryAudio, "Loaded %s: %i samples across %i channels with a sample rate of %i", path.c_str(), clip.sampleCount, clip.channels, clip.sampleRate);

        if (clip.sampleRate != 44100)
            logWarn(WELogCategoryAudio, "Clip %s does not have a sample rate of 44100hz (%i). It will not play back correctly.", path.c_str(), clip.sampleRate);

        clip.id = id;

        loadedClips.insert({ id, clip });

        return loadedClips.at(id);
    }
}
