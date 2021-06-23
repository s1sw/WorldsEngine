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
#include "../Physics/Physics.hpp"

namespace worlds {
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

    AudioSystem::AudioSystem()
        : voices{512}
        , oneshots{64}
        , showDebugMenuVar("a_showDebugMenu", "0") {
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
    float* tempMonoBuffer = nullptr;
    bool copyBuffer = false;

    void AudioSystem::mixVoice(Voice& voice, int numMonoSamplesNeeded, float* stream, AudioSystem* _this) {
        auto& clip = *voice.clip;
        int samplesRemaining = clip.sampleCount - voice.playbackPosition;

        if (samplesRemaining < 0) return;

        int samplesNeeded = std::min(numMonoSamplesNeeded, samplesRemaining);

        float vol = _this->mixerVolumes[static_cast<int>(voice.channel)] * voice.volume;

        // Stereo and non-spatialised mono mixing
        if (clip.channels == 2) {
            for (int i = 0; i < samplesNeeded; i++) {
                int outPos = i * 2;
                int inPos = (i + voice.playbackPosition) * 2;
                stream[outPos] += clip.data[inPos] * vol;
                stream[outPos + 1] += clip.data[inPos + 1] * vol;
            }
        } else if (!voice.spatialise) {
            for (int i = 0; i < samplesNeeded; i++) {
                int outPos = i * 2;
                int inPos = i + voice.playbackPosition;
                stream[outPos] += clip.data[inPos] * vol;
                stream[outPos + 1] += clip.data[inPos] * vol;
            }
        }

        if (voice.spatialise && clip.channels == 1) {
            IPLAudioFormat clipFormat {
                .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
                .channelLayout = IPL_CHANNELLAYOUT_MONO,
                .channelOrder = IPL_CHANNELORDER_INTERLEAVED
            };

            IPLAudioBuffer inBuffer{
                clipFormat, samplesNeeded,
                &clip.data[voice.playbackPosition], nullptr
            };

            IPLAudioBuffer directPathBuffer {
                clipFormat, samplesNeeded,
                tempMonoBuffer, nullptr
            };

            IPLDirectSoundEffectOptions directSoundOptions {
                .applyDistanceAttenuation = IPL_TRUE,
                .applyAirAbsorption = IPL_TRUE,
                .applyDirectivity = IPL_TRUE,
                .directOcclusionMode = IPL_DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY
            };

            iplApplyDirectSoundEffect(voice.iplFx.directSoundEffect,
                inBuffer, voice.spatialInfo.soundPath,
                directSoundOptions, directPathBuffer);

            IPLAudioFormat outFormat {
                .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
                .channelLayout = IPL_CHANNELLAYOUT_STEREO,
                .channelOrder = IPL_CHANNELORDER_INTERLEAVED
            };

            IPLAudioBuffer outBuffer{
                outFormat, samplesNeeded,
                tempBuffer, nullptr
            };

            IPLVector3 dir {
                voice.spatialInfo.direction.x,
                voice.spatialInfo.direction.y,
                voice.spatialInfo.direction.z
            };

            iplApplyBinauralEffect(
                    voice.iplFx.binauralEffect,
                    _this->binauralRenderer,
                    directPathBuffer, dir,
                    IPL_HRTFINTERPOLATION_BILINEAR, 1.0f, outBuffer);

            for (int i = 0; i < samplesNeeded; i++) {
                stream[i * 2 + 0] += ((float*)tempBuffer)[i * 2 + 0] * vol;
                stream[i * 2 + 1] += ((float*)tempBuffer)[i * 2 + 1] * vol;
            }
        }

        voice.playbackPosition += numMonoSamplesNeeded;

        if (voice.playbackPosition >= clip.sampleCount) {
            if (voice.loop) {
                voice.playbackPosition = 0;
            } else {
                voice.isPlaying = false;
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
        //                        number of samples / number of channels / sample rate
        float secondBufferLength = (float)streamLen / (float)_this->channelCount / (float)_this->sampleRate;

        int numMonoSamplesNeeded = streamLen / 2;

        for (int i = 0; i < streamLen; i++) {
            stream[i] = 0.0f;
        }

        for (size_t i = 0; i < _this->voices.size(); i++) {
            if (_this->voices[i].isPlaying)
                mixVoice(_this->voices[i], numMonoSamplesNeeded, stream, _this);
        }

        for (int i = 0; i < streamLen; i++) {
            stream[i] *= _this->volume;
        }

        // for debugging only! otherwise this will unnecessarily slow down
        // the audio thread
        if (lastBuffer && copyBuffer)
            memcpy(lastBuffer, stream, len);

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

    IPLMaterial mainMaterial{0.10f, 0.20f, 0.30f, 0.05f, 0.750f, 0.10f, 0.050f};

    void checkErr(IPLerror err) {
        if (err != IPL_STATUS_SUCCESS) {
            fatalErr("IPL fail");
        }
    }

    void closestHit(const IPLfloat32* iplOrigin, const IPLfloat32* iplDirection,
        const IPLfloat32 minDistance, const IPLfloat32 maxDistance, IPLfloat32* hitDistance, IPLfloat32* hitNormal,
        IPLMaterial** hitMaterial, IPLvoid* userData) {
        glm::vec3 origin {iplOrigin[0], iplOrigin[1], iplOrigin[2]};
        glm::vec3 direction {iplDirection[0], iplDirection[1], iplDirection[2]};

        origin += direction * minDistance;
        RaycastHitInfo hitInfo;

        if (raycast(origin, glm::normalize(direction), maxDistance, &hitInfo, PLAYER_PHYSICS_LAYER)) {
            *hitDistance = hitInfo.distance;

            for (int i = 0; i < 3; i++)
                hitNormal[i] = hitInfo.normal[i];

            *hitMaterial = &mainMaterial;
        } else {
            *hitDistance = INFINITY;
        }
    }

    void anyHit(const IPLfloat32* iplOrigin, const IPLfloat32* iplDirection,
        const IPLfloat32 minDistance, const IPLfloat32 maxDistance, IPLint32* hitExists, IPLvoid* userData) {
        glm::vec3 origin {iplOrigin[0], iplOrigin[1], iplOrigin[2]};
        glm::vec3 direction {iplDirection[0], iplDirection[1], iplDirection[2]};

        if (glm::length2(direction) <= 0.0001f) {
            *hitExists = false;
            return;
        }

        origin += direction * minDistance;

        RaycastHitInfo hitInfo;
        *hitExists = raycast(origin, glm::normalize(direction), maxDistance, &hitInfo, PLAYER_PHYSICS_LAYER);
    }

    AudioSystem* AudioSystem::instance;

    void AudioSystem::initialise(entt::registry& reg) {
        instance = this;
        missingClip = loadAudioClip(AssetDB::pathToId("Audio/SFX/missing.ogg"));

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
        devId = SDL_OpenAudioDevice(nullptr, false, &want, &have, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

        lastBuffer = (float*)malloc(have.samples * have.channels * sizeof(float));
        tempBuffer = (float*)malloc(have.samples * have.channels * sizeof(float));
        tempMonoBuffer = (float*)malloc(have.samples * sizeof(float));

        logMsg(WELogCategoryAudio, "Opened audio device at %ihz with %i channels and %i samples", have.freq, have.channels, have.samples);
        channelCount = have.channels;
        numSamples = have.samples;
        sampleRate = have.freq;

        reg.on_construct<AudioSource>().connect<&AudioSystem::onAudioSourceConstruct>(*this);
        reg.on_destroy<AudioSource>().connect<&AudioSystem::onAudioSourceDestroy>(*this);

        logMsg(WELogCategoryAudio, "Initialising phonon");
        checkErr(iplCreateContext((IPLLogFunction)phLog, nullptr, nullptr, &phononContext));

        IPLRenderingSettings settings{ have.freq, have.samples, IPL_CONVOLUTIONTYPE_PHONON };

        IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, nullptr };
        checkErr(iplCreateBinauralRenderer(phononContext, settings, hrtfParams, &binauralRenderer));

        checkErr(
            iplCreateScene(
                phononContext, nullptr,
                IPL_SCENETYPE_CUSTOM,
                1, &mainMaterial,
                closestHit, anyHit,
                nullptr, nullptr,
                nullptr, &sceneHandle
            )
        );

        IPLSimulationSettings simulationSettings{
            .sceneType = IPL_SCENETYPE_PHONON,
            .maxNumOcclusionSamples = 32,
            .numRays = 1024,
            .numBounces = 2,
            .numThreads = 8,
            .irDuration = 0.5f,
            .ambisonicsOrder = 0,
            .maxConvolutionSources = 32,
            .bakingBatchSize = 1,
            .irradianceMinDistance = 0.3f
        };

        checkErr(iplCreateEnvironment(phononContext, NULL,
                simulationSettings, sceneHandle, NULL, &environment));

        for (auto& v : voices) {
            v.isPlaying = false;
            v.loop = false;
            v.volume = 0.0f;
            v.clip = nullptr;
            v.lock = false;

            IPLAudioFormat audioIn;
            audioIn.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            audioIn.channelLayout = IPL_CHANNELLAYOUT_MONO;
            audioIn.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

            IPLAudioFormat audioOut;
            audioOut.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            audioOut.channelLayout = IPL_CHANNELLAYOUT_STEREO;
            audioOut.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

            IPLRenderingSettings settings{
                sampleRate,
                numSamples,
                IPL_CONVOLUTIONTYPE_PHONON
            };

            checkErr(iplCreateDirectSoundEffect(audioIn, audioIn, settings, &v.iplFx.directSoundEffect));
            checkErr(iplCreateBinauralEffect(binauralRenderer, audioIn, audioOut, &v.iplFx.binauralEffect));
        }

        if (devId == 0) {
            logWarn(WELogCategoryAudio, "Failed to open audio device");
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

        PerfTimer timer;
        SDL_LockAudioDevice(devId);

        reg.view<AudioSource, AudioTrigger, Transform>().each(
            [&](entt::entity ent, AudioSource& as, AudioTrigger& at, Transform& t) {
                glm::vec3 ma = t.position + t.scale;
                glm::vec3 mi = t.position - t.scale;

                bool insideTrigger = glm::all(glm::lessThan(listenerPos, ma)) &&
                    glm::all(glm::greaterThan(listenerPos, mi));

                if (insideTrigger) {
                    if (at.playOnce && at.hasPlayed) return;

                    bool justEntered = !as.isPlaying;

                    if (justEntered && at.resetOnEntry) {
                        voices[internalAs.at(ent).voiceIdx].playbackPosition = 0;
                    }

                    as.isPlaying = true;
                    at.hasPlayed = true;
                } else if (!at.playOnce) {
                    as.isPlaying = false;
                }
        });

        reg.view<Transform, AudioSource>().each(
            [&](entt::entity ent, Transform& transform, AudioSource& audioSource) {
                AudioSourceInternal& asi = internalAs.at(ent);
                Voice& v = voices[asi.voiceIdx];
                v.volume = audioSource.volume;
                auto it = loadedClips.find(audioSource.clipId);
                if (it != loadedClips.end())
                    v.clip = &loadedClips.at(audioSource.clipId);
                else
                    v.clip = &missingClip;

                glm::vec3 dirVec = listenerPos - transform.position;
                v.spatialInfo.direction = glm::normalize(dirVec) * listenerRot;
                v.spatialInfo.distance = glm::length(dirVec);

                v.loop = audioSource.loop;
                v.spatialise = audioSource.spatialise;
                v.channel = audioSource.channel;

                if (audioSource.isPlaying != asi.lastPlaying) {
                    v.isPlaying = audioSource.isPlaying;
                    asi.lastPlaying = audioSource.isPlaying;
                } else {
                    audioSource.isPlaying = v.isPlaying;
                    asi.lastPlaying = v.isPlaying;
                }

                if (v.spatialise) {
                    IPLDistanceAttenuationModel distanceAttenuationModel {
                        .type = IPL_DISTANCEATTENUATION_DEFAULT
                    };

                    IPLAirAbsorptionModel airAbsorptionModel {
                        .type = IPL_AIRABSORPTION_DEFAULT
                    };

                    IPLSource src {
                        .position = convVec(transform.position),
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

                    v.spatialInfo.soundPath = iplGetDirectSoundPath(environment,
                        convVec(listenerPos),
                        convVec(listenerRot * glm::vec3 { 0.0f, 0.0f, 1.0f }),
                        convVec(listenerRot * glm::vec3 { 0.0f, 1.0f, 0.0f }),
                        src,
                        5.0f,
                        64,
                        IPL_DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY,
                        IPL_DIRECTOCCLUSION_VOLUMETRIC);
                }
            }
        );

        oneshots.erase(std::remove_if(oneshots.begin(), oneshots.end(),
            [&](PlayingOneshot& clipInfo) {
                Voice& v = voices[clipInfo.voiceIdx];
                return v.playbackPosition > v.clip->sampleCount;
            }
        ), oneshots.end());

        for (auto& c : oneshots) {
            Voice& v = voices[c.voiceIdx];
            glm::vec3 dirVec = listenerPos - c.location;
            v.spatialInfo.direction = glm::normalize(glm::normalize(dirVec) * listenerRot);
            v.spatialInfo.distance = glm::length(dirVec);

            if (v.spatialise) {
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

                v.spatialInfo.soundPath = iplGetDirectSoundPath(environment,
                    convVec(listenerPos),
                    convVec(listenerRot * glm::vec3 { 0.0f, 0.0f, 1.0f }),
                    convVec(listenerRot * glm::vec3 { 0.0f, 1.0f, 0.0f }),
                    src,
                    5.0f,
                    64,
                    IPL_DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY,
                    IPL_DIRECTOCCLUSION_VOLUMETRIC
                );
            }
        }

        SDL_UnlockAudioDevice(devId);

        double timerMs = timer.stopGetMs();
        if (showDebugMenuVar.getInt()) {
            if (ImGui::Begin("Audio Testing")) {
                ImColor col = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
                if (cpuUsage > 0.5f) {
                    col = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
                }
                ImGui::TextColored(col, "Audio Thread Usage: %.2f%%", cpuUsage * 100.0f);
                ImGui::Text("Main thread audio update: %.2fms", timerMs);
                ImGui::Text("Playing Clip Count: %zu", reg.view<AudioSource>().size());
                ImGui::Text("Playing one shot count: %zu", oneshots.numElements());
                ImGui::Text("Buffer length: %u", numSamples);
                ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

                if (copyBuffer) {
                    ImGui::PlotLines("L Audio", [](void* data, int idx) {
                        return ((float*)data)[idx * 2];
                    }, lastBuffer, numSamples / 2, 0, nullptr, -1.0f, 1.0f, ImVec2(300, 150));

                    ImGui::PlotLines("R Audio", [](void* data, int idx) {
                        return ((float*)data)[idx * 2 + 1];
                    }, lastBuffer, numSamples / 2, 0, nullptr, -1.0f, 1.0f, ImVec2(300, 150));
                }

                for (auto& os : oneshots) {
                    ImGui::Text("Oneshot: %.3f, %.3f, %.3f", os.location.x, os.location.y, os.location.z);
                }

                for (size_t i = 0; i < voices.size(); i++) {
                    if (voices[i].isPlaying) {
                        ImGui::Separator();
                        ImGui::Text("Playback position: %i/%i", voices[i].playbackPosition, voices[i].clip->sampleCount);
                        ImGui::Text("Volume: %f", voices[i].volume);
                        if (voices[i].spatialise) {
                            ImGui::Text("Spatial Info: ");
                            ImGui::Text(" - Distance: %.3f", voices[i].spatialInfo.distance);
                            ImGui::Text(" - Direction: %.3f, %.3f, %.3f", voices[i].spatialInfo.direction.x, voices[i].spatialInfo.direction.y, voices[i].spatialInfo.direction.z);
                        }
                    }
                }
            }

            ImGui::End();
        }
    }

    void AudioSystem::setPauseState(bool paused) {
        isPaused = paused;
        SDL_PauseAudioDevice(devId, paused);
    }

    void AudioSystem::shutdown(entt::registry& reg) {
        reg.on_construct<AudioSource>().disconnect<&AudioSystem::onAudioSourceConstruct>(*this);
        reg.on_destroy<AudioSource>().disconnect<&AudioSystem::onAudioSourceDestroy>(*this);
        SDL_CloseAudioDevice(devId);
        free(tempBuffer);
        free(lastBuffer);
        free(tempMonoBuffer);
    }

    void AudioSystem::resetPlaybackPositions() {
        for (size_t i = 0; i < voices.size(); i++) {
            voices[i].playbackPosition = 0;
        }
    }

    void AudioSystem::playOneShotClip(AssetID id, glm::vec3 location, bool spatialise, float volume, MixerChannel channel) {
        if (loadedClips.count(id) == 0)
            loadAudioClip(id);
        SDL_LockAudioDevice(devId);

        uint32_t voiceIdx = allocateVoice();
        Voice& v = voices[voiceIdx];
        v.isPlaying = true;
        v.spatialise = spatialise;
        v.volume = volume;
        v.loop = false;
        v.channel = channel;
        auto it = loadedClips.find(id);
        if (it != loadedClips.end())
            v.clip = &loadedClips.at(id);
        else
            v.clip = &missingClip;

        PlayingOneshot po {
            .location = location,
            .voiceIdx = (uint32_t)voiceIdx
        };

        oneshots.add(po);

        SDL_UnlockAudioDevice(devId);
    }

    uint32_t AudioSystem::allocateVoice() {
        uint32_t i = 0;

        for (auto& v : voices) {
            if (!v.lock && !v.isPlaying) {
                v.playbackPosition = 0;
                return i;
            }
            i++;
        }

        return ~0u;
    }

    void AudioSystem::onAudioSourceConstruct(entt::registry& reg, entt::entity ent) {
        SDL_LockAudioDevice(devId);

        AudioSource& as = reg.get<AudioSource>(ent);
        AudioSourceInternal asi {
            .voiceIdx = allocateVoice(),
            .lastPlaying = false
        };

        voices[asi.voiceIdx].lock = true;

        internalAs.insert({ ent, asi });

        loadAudioClip(as.clipId).refCount++;
        SDL_UnlockAudioDevice(devId);
    }

    void AudioSystem::onAudioSourceDestroy(entt::registry& reg, entt::entity ent) {
        auto& as = reg.get<AudioSource>(ent);
        SDL_LockAudioDevice(devId);

        auto lcIter = loadedClips.find(as.clipId);

        if (lcIter != loadedClips.end()) {
            LoadedClip& lc = loadedClips.at(as.clipId);
            lc.refCount--;

            if (lc.refCount <= 0) {
                loadedClips.erase(lc.id);
            }
        }

        auto& lsi = internalAs.at(ent);
        voices[lsi.voiceIdx].lock = false;
        voices[lsi.voiceIdx].isPlaying = false;

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
        std::string path = AssetDB::idToPath(id);

        int64_t fLen;
        Result<void*, IOError> res = LoadFileToBuffer(path, &fLen);

        if (res.error != IOError::None) {
            logErr(WELogCategoryAudio, "Could not load audio clip %s", path.c_str());
            return missingClip;
        }

        LoadedClip clip;

        // Decode
        int error;
        stb_vorbis* vorb = stb_vorbis_open_memory(
                (const uint8_t*)res.value, fLen, &error, nullptr);

        if (vorb == nullptr) {
            logErr(WELogCategoryAudio, "Couldn't decode audio clip %s: error %i (%s)", path.c_str(), error, errorStrings.at(error));
            return missingClip;
        }

        decodeVorbis(vorb, clip);

        std::free(res.value);

        logMsg(WELogCategoryAudio, "Loaded %s: %i samples across %i channels with a sample rate of %i", path.c_str(), clip.sampleCount, clip.channels, clip.sampleRate);

        if (clip.sampleRate != 44100) {
            SDL_AudioCVT cvt;
            SDL_BuildAudioCVT(
                &cvt,
                AUDIO_F32,
                clip.channels,
                clip.sampleRate,
                AUDIO_F32,
                clip.channels,
                44100
            );

            cvt.buf = (Uint8*)malloc(cvt.len * cvt.len_mult);
            memcpy(cvt.buf, clip.data, cvt.len);

            SDL_ConvertAudio(&cvt);
            memcpy(clip.data, cvt.buf, cvt.len);
            clip.sampleRate = 44100;
            free(cvt.buf);
        }

        clip.id = id;

        loadedClips.insert({ id, clip });

        return loadedClips.at(id);
    }
}
