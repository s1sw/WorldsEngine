#include <SDL_audio.h>
#include "../Core/Log.hpp"
#include "Audio.hpp"
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include "../Util/TimingUtil.hpp"
#include "../IO/IOUtil.hpp"
#include "../ImGui/imgui.h"
#include "../Core/Transform.hpp"
#include "phonon.h"
#include "../Core/Fatal.hpp"
#include <slib/DynamicLibrary.hpp>
#include "../Physics/Physics.hpp"
#include <fmod_errors.h>
#include <stdlib.h>
#include <glm/gtc/type_ptr.hpp>
#include <Util/EnumUtil.hpp>
#include <Core/JobSystem.hpp>

#define FMCHECK(_result) checkFmodErr(_result, __FILE__, __LINE__)
#define SACHECK(_result) checkSteamAudioErr(_result, __FILE__, __LINE__)

namespace worlds {
    void checkFmodErr(FMOD_RESULT result, const char* file, int line) {
        if (result != FMOD_OK) {
#ifdef DEBUG
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "FMOD error: %s", FMOD_ErrorString(result));
            fatalErrInternal(buffer, file, line);
#else
            logErr("FMOD error: %s", FMOD_ErrorString(result));
#endif
        }
    }

    void checkSteamAudioErr(IPLerror result, const char* file, int line) {
        if (result != IPLerror::IPL_STATUS_SUCCESS) {
            const char* iplErrs[] = {
                "The operation completed successfully.",
                "An unspecified error occurred.",
                "The system ran out of memory."
                "An error occurred while initializing an external dependency."
            };

            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Steam Audio error: %s", iplErrs[(int)result]);
            fatalErrInternal(buffer, file, line);
        }
    }

    FMOD_RESULT convertPhysFSError(PHYSFS_ErrorCode errCode) {
        switch (errCode) {
        case PHYSFS_ERR_NOT_FOUND:
            return FMOD_ERR_FILE_NOTFOUND;
        case PHYSFS_ERR_OUT_OF_MEMORY:
            return FMOD_ERR_MEMORY;
        case PHYSFS_ERR_OK:
            return FMOD_OK;
        default:
            return FMOD_ERR_FILE_BAD;
        }
    }

    FMOD_RESULT F_CALL fileOpenCallback(const char* name, unsigned int* filesize, void** handle, void* userdata) {
        PHYSFS_File* file = PHYSFS_openRead(name);

        if (file == nullptr) {
            PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();

            return convertPhysFSError(err);
        }

        *handle = file;
        *filesize = (uint32_t)PHYSFS_fileLength(file);

        return FMOD_OK;
    }

    FMOD_RESULT F_CALL fileCloseCallback(void* handle, void* userdata) {
        if (PHYSFS_close((PHYSFS_File*)handle) == 0)
            return convertPhysFSError(PHYSFS_getLastErrorCode());
        else
            return FMOD_OK;
    }

    FMOD_RESULT F_CALL fileReadCallback(void* handle, void* buffer, uint32_t sizeBytes, uint32_t* bytesRead, void* userdata) {
        int64_t result = PHYSFS_readBytes((PHYSFS_File*)handle, buffer, sizeBytes);
        if (result == -1) {
            PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
            *bytesRead = 0;

            return convertPhysFSError(err);
        } else {
            *bytesRead = result;
            return FMOD_OK;
        }
    }

    FMOD_RESULT F_CALL fileSeekCallback(void* handle, uint32_t pos, void* userdata) {
        if (PHYSFS_seek((PHYSFS_File*)handle, pos) == 0)
            return convertPhysFSError(PHYSFS_getLastErrorCode());
        else
            return FMOD_OK;
    }

    AudioSystem* AudioSystem::instance;

    typedef void(*PFN_iplFMODInitialize)(IPLContext context);
    typedef void(*PFN_iplFMODSetHRTF)(IPLHRTF hrtf);
    typedef void(*PFN_iplFMODSetSimulationSettings)(IPLSimulationSettings simulationSettings);
    typedef void(*PFN_iplFMODSetReverbSource)(IPLSource reverbSource);

    FMOD_RESULT F_CALL fmodDebugCallback(FMOD_DEBUG_FLAGS flags, const char* file, int line, const char* func, const char* message) {
        if (flags & FMOD_DEBUG_LEVEL_ERROR) {
            logErr(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        if (flags & FMOD_DEBUG_LEVEL_WARNING) {
            logWarn(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        if (flags & FMOD_DEBUG_LEVEL_LOG) {
            logVrb(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        return FMOD_OK;
    }

    void IPLCALL steamAudioDebugCallback(IPLLogLevel logLevel, const char* message) {
        logMsg(WELogCategoryAudio, "%s", message);
    }

    void* IPLCALL steamAudioAllocAligned(IPLsize size, IPLsize alignment) {
#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        return std::aligned_alloc(alignment, size);
#endif
    }

    void IPLCALL steamAudioFreeAligned(void* memBlock) {
#ifdef _WIN32
        _aligned_free(memBlock);
#else
        std::free(memBlock);
#endif
    }

    void AudioSource::changeEventPath(const std::string_view& eventPath) {
        FMOD::Studio::EventDescription* desc;

        AudioSystem* _this = AudioSystem::getInstance();

        FMOD_RESULT result;
        result = _this->studioSystem->getEvent(eventPath.data(), &desc);
        if (result != FMOD_OK) {
            logErr(WELogCategoryAudio, "Failed to get event %s: %s", eventPath.data(), FMOD_ErrorString(result));
            return;
        }

        if (eventInstance != nullptr) {
            eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            eventInstance->release();
        }

        result = desc->createInstance(&eventInstance);
        if (result != FMOD_OK) {
            logErr(WELogCategoryAudio, "Failed to create event %s: %s", eventPath.data(), FMOD_ErrorString(result));
            return;
        }

        _eventPath.assign(eventPath);
    }

    FMOD_STUDIO_PLAYBACK_STATE AudioSource::playbackState() {
        FMOD_STUDIO_PLAYBACK_STATE ret;
        FMCHECK(eventInstance->getPlaybackState(&ret));
        return ret;
    }

    std::atomic<bool> simRunning = false;
    std::atomic<bool> simThreadKickoff;
    std::condition_variable simConVar;
    std::mutex simMutex;
    std::thread steamAudioSimThread;
    std::atomic<bool> simThreadAlive = true;
    
    void simThread(IPLSimulator simulator) {
        while (simThreadAlive) {
            {
                std::unique_lock lock{ simMutex };
                simConVar.wait(lock, []() { return simThreadKickoff.load(); });
            }

            simRunning = true;
            iplSimulatorRunReflections(simulator);
            iplSimulatorRunPathing(simulator);
            simRunning = false;
        }
    }

    AudioSystem::AudioSystem() {
        instance = this;
        const char* phononPluginName;

#ifdef _WIN32
        phononPluginName = "phonon_fmod.dll";
#elif defined(__linux__)
        phononPluginName = "libphonon_fmod.so";
#endif
        //FMCHECK(FMOD::Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_CALLBACK, fmodDebugCallback));
        //size_t fmodHeapSize = 100000 * 2 * 512;
        //void* fmodHeap = malloc(fmodHeapSize);
        //FMOD::Memory_Initialize(fmodHeap, 20000 * 2 * 512, nullptr, nullptr, nullptr);

        FMCHECK(FMOD::Studio::System::create(&studioSystem));
        FMCHECK(studioSystem->getCoreSystem(&system));
        FMCHECK(system->setSoftwareFormat(0, FMOD_SPEAKERMODE_STEREO, 0));

        FMOD_RESULT result = studioSystem->initialize(2048, FMOD_STUDIO_INIT_NORMAL | FMOD_STUDIO_INIT_LIVEUPDATE, FMOD_INIT_NORMAL, nullptr);

        if (result == FMOD_ERR_OUTPUT_INIT) {
            logErr(WELogCategoryAudio, "Initializing the audio output device failed");
            available = false;
            return;
        } else {
            FMCHECK(result);
        }

        FMCHECK(studioSystem->setNumListeners(1));

        FMCHECK(system->setFileSystem(fileOpenCallback, fileCloseCallback, fileReadCallback, fileSeekCallback, nullptr, nullptr, -1));

        FMCHECK(system->loadPlugin(phononPluginName, &phononPluginHandle));

        // Get Steam Audio's setting equivalents from FMOD
        IPLAudioSettings audioSettings{};

        {
            FMOD_SPEAKERMODE speakerMode;
            int numRawSpeakers;
            system->getSoftwareFormat(&audioSettings.samplingRate, &speakerMode, &numRawSpeakers);

            int numBuffers;
            uint32_t bufferLen;
            system->getDSPBufferSize(&bufferLen, &numBuffers);
            audioSettings.frameSize = bufferLen;
        }

        // Load function pointers for the Steam Audio FMOD plugin
        slib::DynamicLibrary fmodPlugin(phononPluginName);

        PFN_iplFMODInitialize iplFMODInitialize;
        PFN_iplFMODSetHRTF iplFMODSetHRTF;
        PFN_iplFMODSetSimulationSettings iplFMODSetSimulationSettings;
        PFN_iplFMODSetReverbSource iplFMODSetReverbSource;

        iplFMODInitialize = (PFN_iplFMODInitialize)fmodPlugin.getFunctionPointer("iplFMODInitialize");
        iplFMODSetHRTF = (PFN_iplFMODSetHRTF)fmodPlugin.getFunctionPointer("iplFMODSetHRTF");
        iplFMODSetSimulationSettings = (PFN_iplFMODSetSimulationSettings)fmodPlugin.getFunctionPointer("iplFMODSetSimulationSettings");
        iplFMODSetReverbSource = (PFN_iplFMODSetReverbSource)fmodPlugin.getFunctionPointer("iplFMODSetReverbSource");

        // Create the Steam Audio context
        IPLContextSettings contextSettings{};
        contextSettings.version = STEAMAUDIO_VERSION;
        contextSettings.simdLevel = IPL_SIMDLEVEL_SSE4;
        contextSettings.logCallback = steamAudioDebugCallback;
        contextSettings.allocateCallback = steamAudioAllocAligned;
        contextSettings.freeCallback = steamAudioFreeAligned;

        SACHECK(iplContextCreate(&contextSettings, &phononContext));

        // Create HRTF
        IPLHRTFSettings hrtfSettings{};
        hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

        iplFMODInitialize(phononContext);
        SACHECK(iplHRTFCreate(phononContext, &audioSettings, &hrtfSettings, &phononHrtf));
        iplFMODSetHRTF(phononHrtf);

        IPLSimulationSettings simulationSettings{};
        simulationSettings.flags = (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_REFLECTIONS);
        simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
        simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        simulationSettings.maxNumRays = 4096;
        simulationSettings.maxNumOcclusionSamples = 1024;
        simulationSettings.numDiffuseSamples = 32;
        simulationSettings.maxDuration = 2.0f;
        simulationSettings.maxOrder = 2;
        simulationSettings.maxNumSources = 512;
        simulationSettings.numThreads = 8;
        simulationSettings.rayBatchSize = 16;
        simulationSettings.numVisSamples = 512;
        simulationSettings.samplingRate = audioSettings.samplingRate;
        simulationSettings.frameSize = audioSettings.frameSize;

        SACHECK(iplSimulatorCreate(phononContext, &simulationSettings, &simulator));

        IPLSourceSettings sourceSettings{};
        sourceSettings.flags = simulationSettings.flags;
        listenerCentricSource = nullptr;
        SACHECK(iplSourceCreate(simulator, &sourceSettings, &listenerCentricSource));

        iplSourceAdd(listenerCentricSource, simulator);

        // Create a blank scene
        IPLSceneSettings sceneSettings{};
        sceneSettings.type = IPL_SCENETYPE_DEFAULT;
        SACHECK(iplSceneCreate(phononContext, &sceneSettings, &scene));

        iplSimulatorSetScene(simulator, scene);
        iplSimulatorCommit(simulator);

        iplFMODSetSimulationSettings(simulationSettings);
        iplFMODSetReverbSource(listenerCentricSource);

        lastListenerPos = glm::vec3(0.0f, 0.0f, 0.0f);
        steamAudioSimThread = std::thread([=]() {simThread(simulator); });
    }

    void AudioSystem::initialise(entt::registry& worldState) {
        worldState.on_destroy<AudioSource>().connect<&AudioSystem::onAudioSourceDestroy>(*this);
        g_console->registerCommand([&](void*, const char* arg) {
            if (!available) {
                logErr(WELogCategoryAudio, "Audio subsystem is unavailable");
                return;
            }

            float vol = std::atof(arg);
            if (!masterVCA->isValid()) {
                logErr(WELogCategoryAudio, "Master VCA handle was invalid");
            } else {
                masterVCA->setVolume(vol);
            }
        }, "a_setMasterVolume", "Sets the master audio volume.");

        g_console->registerCommand([&](void*, const char*) {
            if (!available) {
                logErr(WELogCategoryAudio, "Audio subsystem is unavailable");
                return;
            }

            updateAudioScene(worldState);
        }, "a_forceUpdateAudioScene", "Forces an update of the Steam Audio scene.");
    }

    void AudioSystem::onAudioSourceDestroy(entt::registry& reg, entt::entity entity) {
        if (!available) return;
        AudioSource& as = reg.get<AudioSource>(entity);

        if (as.eventInstance) {
            FMCHECK(as.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
            FMCHECK(as.eventInstance->release());
        }
    }

    void AudioSystem::loadMasterBanks() {
        if (!available) return;
        masterBank = loadBank("FMOD/Desktop/Master.bank");
        stringsBank = loadBank("FMOD/Desktop/Master.strings.bank");
        if (!masterBank) return;
        FMCHECK(studioSystem->getVCA("vca:/Master", &masterVCA));
    }

    FMOD_VECTOR convVec(glm::vec3 v3) {
        FMOD_VECTOR v{};
        v.x = -v3.x;
        v.y = v3.y;
        v.z = v3.z;
        return v;
    }

    IPLVector3 convVecSA(glm::vec3 v3) {
        IPLVector3 v{};
        v.x = v3.x;
        v.y = v3.y;
        v.z = v3.z;
        return v;
    }

    void AudioSystem::update(entt::registry& worldState, glm::vec3 listenerPos, glm::quat listenerRot, float deltaTime) {
        if (!available) return;
        glm::vec3 movement = listenerPos - lastListenerPos;
        movement /= deltaTime;

        if (glm::any(glm::isinf(movement)) || glm::any(glm::isnan(movement)))
            movement = glm::vec3{ 0.0f };

        if (glm::any(glm::isnan(listenerPos)))
            listenerPos = glm::vec3{ 0.0f };

        FMOD_3D_ATTRIBUTES listenerAttributes{};
        listenerAttributes.forward = convVec(listenerRot * glm::vec3(0.0f, 0.0f, 1.0f));
        listenerAttributes.up = convVec(listenerRot * glm::vec3(0.0f, 1.0f, 0.0f));

        listenerAttributes.position = convVec(listenerPos);
        listenerAttributes.velocity = convVec(movement);

        IPLSimulationFlags simFlags = (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);

        IPLSimulationInputs inputs{};
        inputs.flags = simFlags;
        inputs.source.right = convVecSA(listenerRot * glm::vec3(-1.0f, 0.0f, 0.0f));
        inputs.source.up = convVecSA(listenerRot * glm::vec3(0.0f, 1.0f, 0.0f));
        inputs.source.ahead = convVecSA(listenerRot * glm::vec3(0.0f, 0.0f, -1.0f));
        inputs.source.origin = convVecSA(listenerPos);
        inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
        inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
        inputs.reverbScale[0] = 1.0f;
        inputs.reverbScale[1] = 1.0f;
        inputs.reverbScale[2] = 1.0f;
        inputs.hybridReverbTransitionTime = 1.0f;
        inputs.hybridReverbOverlapPercent = 0.25f;
        inputs.baked = IPL_FALSE;

        iplSourceSetInputs(listenerCentricSource, simFlags, &inputs);

        IPLSimulationSharedInputs sharedInputs{};
        sharedInputs.listener = inputs.source;
        sharedInputs.numRays = 2048;
        sharedInputs.numBounces = 16;
        sharedInputs.duration = 2.0f;
        sharedInputs.order = 2;
        sharedInputs.irradianceMinDistance = 1.0f;

        iplSimulatorSetSharedInputs(simulator, simFlags, &sharedInputs);

        iplSimulatorRunDirect(simulator);

        timeSinceLastSim += deltaTime;
        if (timeSinceLastSim > 0.1f && !simRunning) {
            simThreadKickoff = true;
            simConVar.notify_one();
            timeSinceLastSim = 0.0f;
        }

        worldState.view<AudioSource, Transform>().each([](AudioSource& as, Transform& t) {
            if (as.eventInstance == nullptr) return;

            FMOD_3D_ATTRIBUTES sourceAttributes{};
            sourceAttributes.position = convVec(t.position);
            sourceAttributes.forward = convVec(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
            sourceAttributes.up = convVec(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));

            FMCHECK(as.eventInstance->set3DAttributes(&sourceAttributes));
        });

        FMCHECK(studioSystem->setListenerAttributes(0, &listenerAttributes, &listenerAttributes.position));

        for (AttachedOneshot& ao : attachedOneshots) {
            if (!worldState.valid(ao.entity)) {
                ao.markForRemoval = true;
                FMCHECK(ao.instance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
                FMCHECK(ao.instance->release());
                continue;
            }

            FMOD_STUDIO_PLAYBACK_STATE playbackState;
            FMCHECK(ao.instance->getPlaybackState(&playbackState));

            if (playbackState == FMOD_STUDIO_PLAYBACK_STOPPED) {
                ao.markForRemoval = true;
                FMCHECK(ao.instance->release());
                continue;
            }

            Transform& t = worldState.get<Transform>(ao.entity);

            FMOD_3D_ATTRIBUTES sourceAttributes{};
            sourceAttributes.position = convVec(t.position);
            sourceAttributes.forward = convVec(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
            sourceAttributes.up = convVec(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
            sourceAttributes.velocity = convVec((t.position - ao.lastPosition) / deltaTime);

            ao.lastPosition = t.position;

            FMCHECK(ao.instance->set3DAttributes(&sourceAttributes));
        }

        attachedOneshots.erase(
            std::remove_if(attachedOneshots.begin(), attachedOneshots.end(), [](AttachedOneshot& ao) { return ao.markForRemoval; }),
            attachedOneshots.end()
        );

        FMCHECK(studioSystem->update());
    }

    void AudioSystem::stopEverything(entt::registry& reg) {
        if (!available) return;
        reg.view<AudioSource>().each([](AudioSource& as) {
            as.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
        });
    }

    void AudioSystem::playOneShotClip(AssetID id, glm::vec3 location, bool spatialise, float volume, MixerChannel) {
        if (!available) return;
        FMOD::Sound* sound;

        if (sounds.contains(id)) {
            sound = sounds.at(id);
        } else {
            std::string path = AssetDB::idToPath(id);

            FMOD_RESULT result;
            result = system->createSound(path.c_str(), FMOD_CREATESAMPLE, nullptr, &sound);

            if (result != FMOD_OK) {
                logErr(WELogCategoryAudio, "Failed to create oneshot clip %s", path.c_str());
                return;
            }

            sounds.insert({ id, sound });
        }

        FMOD::Channel* channel;
        FMCHECK(system->playSound(sound, nullptr, false, &channel));

        if (spatialise) {
            FMOD_VECTOR fmLocation = convVec(location);
            FMOD_VECTOR vel{0.0f, 0.0f, 0.0f};
            channel->set3DAttributes(&fmLocation, &vel);
        }
    }

    void AudioSystem::playOneShotEvent(const char* eventPath, glm::vec3 location, float volume) {
        playOneShotAttachedEvent(eventPath, location, entt::null, volume);
    }

    void AudioSystem::playOneShotAttachedEvent(const char* eventPath, glm::vec3 location, entt::entity attachedEntity, float volume) {
        if (!available) return;
        FMOD_RESULT result;

        FMOD::Studio::EventDescription* desc;
        result = studioSystem->getEvent(eventPath, &desc);

        if (result != FMOD_OK) {
            logErr(WELogCategoryAudio, "Failed to get event %s: %s", eventPath, FMOD_ErrorString(result));
            return;
        }

        int instanceCount = 0;

        FMCHECK(desc->getInstanceCount(&instanceCount));

        if (instanceCount > 100) {
            return;
        }

        FMOD::Studio::EventInstance* instance;

        result = desc->createInstance(&instance);
        if (result != FMOD_OK) {
            logErr(WELogCategoryAudio, "Failed to create instance of event %s: %s", eventPath, FMOD_ErrorString(result));
            return;
        }

        FMOD_3D_ATTRIBUTES attr{};
        attr.position = convVec(location);
        attr.forward = convVec(glm::vec3(0.0f, 0.0f, 1.0f));
        attr.up = convVec(glm::vec3(0.0f, 1.0f, 0.0f));
        attr.velocity = convVec(glm::vec3(0.0f));

        FMCHECK(instance->set3DAttributes(&attr));
        FMCHECK(instance->setVolume(volume));

        FMCHECK(instance->start());

        if (attachedEntity == entt::null)
            FMCHECK(instance->release());
        else {
            AttachedOneshot attachedOneshot{
                .instance = instance,
                .entity = attachedEntity,
                .lastPosition = location
            };

            attachedOneshots.push_back(attachedOneshot);
        }
    }

    void AudioSystem::shutdown(entt::registry& worldState) {
        if (!available) return;
        simThreadAlive = false;
        steamAudioSimThread.join();
        FMCHECK(studioSystem->release());
        worldState.view<AudioSource>().each([](AudioSource& as) {
            FMCHECK(as.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
            FMCHECK(as.eventInstance->release());
        });
    }

    FMOD::Studio::Bank* AudioSystem::loadBank(const char* path) {
        if (!available) return nullptr;

        PHYSFS_getLastErrorCode();
        FMOD::Studio::Bank* bank;

        if (loadedBanks.contains(path))
            return loadedBanks.at(path);

        if (studioSystem->loadBankFile(path, FMOD_STUDIO_LOAD_BANK_NORMAL, &bank) != FMOD_OK) {
            return nullptr;
        }
        loadedBanks.insert({ path, bank });

        return bank;
    }

    void AudioSystem::bakeProbes(entt::registry& registry) {

        IPLProbeBatch probeBatch = nullptr;
        SACHECK(iplProbeBatchCreate(phononContext, &probeBatch));

        std::vector<IPLProbeArray> probeArrays;
        registry.view<ReverbProbeBox, Transform>().each([&](Transform& t) {
            IPLMatrix4x4 iplMat{};
            glm::mat4 tMat = glm::transpose(t.getMatrix());
            memcpy(iplMat.elements, glm::value_ptr(tMat), sizeof(float) * 16);

            IPLProbeGenerationParams probeGenParams{};
            probeGenParams.type = IPL_PROBEGENERATIONTYPE_UNIFORMFLOOR;
            probeGenParams.spacing = 2.0f;
            probeGenParams.height = 1.5f;
            probeGenParams.transform = iplMat;

            IPLProbeArray probeArray = nullptr;
            SACHECK(iplProbeArrayCreate(phononContext, &probeArray));
            iplProbeArrayGenerateProbes(probeArray, scene, &probeGenParams);
            probeArrays.push_back(probeArray);
        });
    }

    void AudioSystem::updateAudioScene(entt::registry& reg) {
        if (scene)
            iplSceneRelease(&scene);

        scene = createScene(reg);
        while (simRunning) {}
        iplSimulatorSetScene(simulator, scene);
        iplSimulatorCommit(simulator);
    }

    IPLScene AudioSystem::createScene(entt::registry& reg) {
        IPLSceneSettings sceneSettings{};
        sceneSettings.type = IPL_SCENETYPE_DEFAULT;

        IPLScene scene = nullptr;
        SACHECK(iplSceneCreate(phononContext, &sceneSettings, &scene));

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            if (!enumHasFlag(wo.staticFlags, StaticFlags::Audio)) return;
            glm::mat4 tMat = t.getMatrix();

            // oh boy
            IPLMaterial mat{{0.1f, 0.2f, 0.3f}, 0.05f, {0.1f, 0.05f, 0.03f}};
            std::vector<IPLVector3> verts;
            std::vector<IPLTriangle> triangles;
            std::vector<int> materialIndices;

            const LoadedMesh& lm = MeshManager::loadOrGet(wo.mesh);

            verts.resize(lm.vertices.size());
            triangles.resize(lm.indices.size() / 3);
            materialIndices.resize(triangles.size());

            for (uint32_t i = 0; i < lm.vertices.size(); i++) {
                glm::vec3 pos = tMat * glm::vec4(lm.vertices[i].position, 1.0f);
                verts[i].x = pos.x;
                verts[i].y = pos.y;
                verts[i].z = pos.z;
            }

            for (uint32_t i = 0; i < lm.indices.size(); i += 3) {
                triangles[i / 3] = { (int)lm.indices[i], (int)lm.indices[i + 1], (int)lm.indices[i + 2] };
            }

            for (uint32_t i = 0; i < materialIndices.size(); i++) {
                materialIndices[i] = 0;
            }

            IPLStaticMeshSettings settings{};
            settings.numVertices = verts.size();
            settings.numTriangles = triangles.size();
            settings.numMaterials = 1;
            settings.vertices = verts.data();
            settings.triangles = triangles.data();
            settings.materialIndices = materialIndices.data();
            settings.materials = &mat;

            IPLStaticMesh mesh = nullptr;
            SACHECK(iplStaticMeshCreate(scene, &settings, &mesh));
            iplStaticMeshAdd(mesh, scene);
            //iplStaticMeshRelease(&mesh);
        });
        iplSceneCommit(scene);

        return scene;
    }
}
