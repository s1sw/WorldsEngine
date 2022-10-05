#include "Audio.hpp"
#include <Core/Fatal.hpp>
#include <Core/MaterialManager.hpp>
#include <Core/MeshManager.hpp>
#include <ImGui/imgui.h>
#include <Libs/IconsFontaudio.h>
#include <fmod_errors.h>
#include <Audio/PhononFmod.hpp>
#include <mutex>
#include <Util/EnumUtil.hpp>
#include <Util/TimingUtil.hpp>
#include <slib/DynamicLibrary.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Tracy.hpp>

#define FMCHECK(_result) checkFmodErr(_result, __FILE__, __LINE__)
#define SACHECK(_result) checkSteamAudioErr(_result, __FILE__, __LINE__)

namespace worlds
{
    void checkFmodErr(FMOD_RESULT result, const char* file, int line)
    {
        if (result != FMOD_OK)
        {
#ifdef DEBUG
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "FMOD error: %s", FMOD_ErrorString(result));
            fatalErrInternal(buffer, file, line);
#else
            logErr("FMOD error: %s (file %s, line %i)", FMOD_ErrorString(result), file, line);
#endif
        }
    }

    void checkSteamAudioErr(IPLerror result, const char* file, int line)
    {
        if (result != IPLerror::IPL_STATUS_SUCCESS)
        {
            const char* iplErrs[] = {"The operation completed successfully.", "An unspecified error occurred.",
                                     "The system ran out of memory."
                                     "An error occurred while initializing an external dependency."};

            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Steam Audio error: %s", iplErrs[(int)result]);
            fatalErrInternal(buffer, file, line);
        }
    }

    FMOD_RESULT convertPhysFSError(PHYSFS_ErrorCode errCode)
    {
        switch (errCode)
        {
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

    FMOD_RESULT F_CALL fileOpenCallback(const char* name, unsigned int* filesize, void** handle, void* userdata)
    {
        PHYSFS_File* file = PHYSFS_openRead(name);

        if (file == nullptr)
        {
            PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();

            return convertPhysFSError(err);
        }

        *handle = file;
        *filesize = (uint32_t)PHYSFS_fileLength(file);

        return FMOD_OK;
    }

    FMOD_RESULT F_CALL fileCloseCallback(void* handle, void* userdata)
    {
        if (PHYSFS_close((PHYSFS_File*)handle) == 0)
            return convertPhysFSError(PHYSFS_getLastErrorCode());
        else
            return FMOD_OK;
    }

    FMOD_RESULT F_CALL fileReadCallback(void* handle, void* buffer, uint32_t sizeBytes, uint32_t* bytesRead,
                                        void* userdata)
    {
        int64_t result = PHYSFS_readBytes((PHYSFS_File*)handle, buffer, sizeBytes);
        if (result == -1)
        {
            PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
            *bytesRead = 0;

            return convertPhysFSError(err);
        }
        else
        {
            *bytesRead = result;
            return FMOD_OK;
        }
    }

    FMOD_RESULT F_CALL fileSeekCallback(void* handle, uint32_t pos, void* userdata)
    {
        if (PHYSFS_seek((PHYSFS_File*)handle, pos) == 0)
            return convertPhysFSError(PHYSFS_getLastErrorCode());
        else
            return FMOD_OK;
    }

    AudioSystem* AudioSystem::instance;

    typedef void (*PFN_iplFMODInitialize)(IPLContext context);
    typedef void (*PFN_iplFMODSetHRTF)(IPLHRTF hrtf);
    typedef void (*PFN_iplFMODSetSimulationSettings)(IPLSimulationSettings simulationSettings);
    typedef void (*PFN_iplFMODSetReverbSource)(IPLSource reverbSource);

    FMOD_RESULT F_CALL fmodDebugCallback(FMOD_DEBUG_FLAGS flags, const char* file, int line, const char* func,
                                         const char* message)
    {
        if (flags & FMOD_DEBUG_LEVEL_ERROR)
        {
            logErr(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        if (flags & FMOD_DEBUG_LEVEL_WARNING)
        {
            logWarn(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        if (flags & FMOD_DEBUG_LEVEL_LOG)
        {
            logVrb(WELogCategoryAudio, "FMOD: %s (%s:%s, %i)", message, file, func, line);
        }

        return FMOD_OK;
    }

    void IPLCALL steamAudioDebugCallback(IPLLogLevel logLevel, const char* message)
    {
        logMsg(WELogCategoryAudio, "%s", message);
    }

    void* IPLCALL steamAudioAllocAligned(IPLsize size, IPLsize alignment)
    {
#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        return std::aligned_alloc(alignment, size);
#endif
    }

    void IPLCALL steamAudioFreeAligned(void* memBlock)
    {
#ifdef _WIN32
        _aligned_free(memBlock);
#else
        std::free(memBlock);
#endif
    }

    FMOD_RESULT findSteamAudioDSP(FMOD::Studio::EventInstance* instance, FMOD::DSP** dsp)
    {
        FMOD::ChannelGroup* channelGroup;
        FMOD_RESULT res = instance->getChannelGroup(&channelGroup);

        if (res != FMOD_OK)
            return res;

        int numDsps;
        res = channelGroup->getNumDSPs(&numDsps);
        if (res != FMOD_OK)
            return res;

        FMOD::DSP* spatializerDsp;
        bool found = false;
        for (int i = 0; i < numDsps; i++)
        {
            res = channelGroup->getDSP(i, &spatializerDsp);
            if (res != FMOD_OK)
                return res;

            char buffer[33] = {0};
            res = spatializerDsp->getInfo(buffer, nullptr, nullptr, nullptr, nullptr);
            if (res != FMOD_OK)
                return res;

            if (strcmp(buffer, "Steam Audio Spatializer") == 0)
            {
                *dsp = spatializerDsp;
                found = true;
            }
        }

        if (!found)
            return FMOD_ERR_DSP_NOTFOUND;

        return FMOD_OK;
    }

    class AudioSystem::SteamAudioSimThread
    {
      public:
        std::mutex commitMutex;

        SteamAudioSimThread(IPLSimulator simulator, AudioSystem* system) : simulator{simulator}, system(system)
        {
            thread = std::thread([this]() { actualThread(); });
        }

        bool isSimRunning()
        {
            return simRunning;
        }
        void runSimulation()
        {
            simThreadKickoff = true;
            conVar.notify_one();
        }

        double lastStepTime() const
        {
            return lastRunTime;
        }

        ~SteamAudioSimThread()
        {
            threadAlive = false;
            simThreadKickoff = true;
            conVar.notify_one();
            thread.join();
        }

      private:
        void actualThread()
        {
            while (threadAlive)
            {
                {
                    std::unique_lock lock{mutex};
                    conVar.wait(lock, [this]() { return simThreadKickoff.load(); });
                    simThreadKickoff.store(false);
                }

                if (system->needsSimCommit)
                {
                    std::unique_lock lock{commitMutex};
                    while (!system->sourcesToAdd.empty())
                    {
                        IPLSource source = system->sourcesToAdd.front();
                        iplSourceAdd(source, simulator);
                        system->sourcesToAdd.pop();
                    }
                    iplSimulatorCommit(simulator);

                    while (!system->sourcesToRemove.empty())
                    {
                        IPLSource source = system->sourcesToRemove.front();
                        iplSourceRemove(source, simulator);
                        iplSourceRelease(&source);
                        system->sourcesToRemove.pop();
                        system->sourcesInSim--;
                    }

                    iplSimulatorCommit(simulator);
                    system->needsSimCommit = false;
                }

                {
                    PerfTimer pt;
                    simRunning = true;
                    iplSimulatorRunReflections(simulator);
                    iplSimulatorRunPathing(simulator);
                    simRunning = false;
                    lastRunTime = pt.stopGetMs();
                }
            }
        }

        std::atomic<bool> simRunning = false;
        std::atomic<bool> simThreadKickoff;
        std::condition_variable conVar;
        std::mutex mutex;
        std::thread thread;
        std::atomic<bool> threadAlive = true;
        IPLSimulator simulator;
        AudioSystem* system;
        double lastRunTime = 0.0;
    };

    FMOD_RESULT audioSourcePhononEventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* cevent,
                                               void* param)
    {
        if (type != FMOD_STUDIO_EVENT_CALLBACK_CREATED)
            return FMOD_OK;

        auto* event = (FMOD::Studio::EventInstance*)cevent;

        IPLSource source;

        FMCHECK(event->getUserData((void**)&source));
        FMOD::DSP* dsp;
        FMCHECK(findSteamAudioDSP(event, &dsp));

        // The FMOD plugin says here that it wants the simulation outputs.
        // However, this is wrong. The code is actually looking for the IPLSource attached
        // to the Steam Audio source!
        FMCHECK(dsp->setParameterData(SpatializerEffect::SIMULATION_OUTPUTS, &source, sizeof(IPLSource)));

        return FMOD_OK;
    }

    void AudioSource::changeEventPath(const std::string_view& eventPath)
    {
        FMOD::Studio::EventDescription* desc;

        AudioSystem* _this = AudioSystem::getInstance();

        FMOD_RESULT result;
        result = _this->studioSystem->getEvent(eventPath.data(), &desc);
        if (result != FMOD_OK)
        {
            logErr(WELogCategoryAudio, "Failed to get event %s: %s", eventPath.data(), FMOD_ErrorString(result));
            return;
        }

        bool createPhononSource = true;

        FMOD_STUDIO_USER_PROPERTY userProp;
        FMOD_RESULT propertyResult = desc->getUserProperty("UsePhononSource", &userProp);

        if (propertyResult == FMOD_ERR_EVENT_NOTFOUND)
        {
            createPhononSource = false;
        }
        else
        {
            FMCHECK(propertyResult);
        }

        if (eventInstance != nullptr)
        {
            eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            eventInstance->release();
        }

        result = desc->createInstance(&eventInstance);
        if (result != FMOD_OK)
        {
            logErr(WELogCategoryAudio, "Failed to create event %s: %s", eventPath.data(), FMOD_ErrorString(result));
            return;
        }

        _eventPath.assign(eventPath);

        if (createPhononSource)
        {
            IPLSourceSettings sourceSettings{
                (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_DIRECT)};

            AudioSystem* as = AudioSystem::getInstance();
            SACHECK(iplSourceCreate(as->simulator, &sourceSettings, &phononSource));
            std::unique_lock lock{_this->simThread->commitMutex};

            eventInstance->setUserData(phononSource);
            eventInstance->setCallback(audioSourcePhononEventCallback, FMOD_STUDIO_EVENT_CALLBACK_CREATED);
        }
    }

    FMOD_STUDIO_PLAYBACK_STATE AudioSource::playbackState()
    {
        FMOD_STUDIO_PLAYBACK_STATE ret;
        FMCHECK(eventInstance->getPlaybackState(&ret));
        return ret;
    }


    ConVar a_showDebugInfo{"a_showDebugInfo", "0"};

    AudioSystem::AudioSystem()
    {
        instance = this;
        const char* phononPluginName;

#ifdef _WIN32
        phononPluginName = "phonon_fmod.dll";
#elif defined(__linux__)
        phononPluginName = "./libphonon_fmod.so";
#endif

        FMCHECK(FMOD::Studio::System::create(&studioSystem));
        FMCHECK(studioSystem->getCoreSystem(&system));
        FMCHECK(system->setSoftwareFormat(0, FMOD_SPEAKERMODE_STEREO, 0));

        FMOD_STUDIO_INITFLAGS studioInitFlags = FMOD_STUDIO_INIT_NORMAL | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE;

        bool useLiveUpdate = EngineArguments::hasArgument("editor") || EngineArguments::hasArgument("fmod-live-update");

        if (useLiveUpdate)
            studioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;

        FMOD_RESULT result = studioSystem->initialize(2048, studioInitFlags, FMOD_INIT_NORMAL, nullptr);

        if (result == FMOD_ERR_OUTPUT_INIT)
        {
            logErr(WELogCategoryAudio, "Initializing the audio output device failed");
            available = false;
            return;
        }
        else
        {
            FMCHECK(result);
        }

        FMCHECK(studioSystem->setNumListeners(1));

        FMCHECK(system->setFileSystem(fileOpenCallback, fileCloseCallback, fileReadCallback, fileSeekCallback, nullptr,
                                      nullptr, -1));

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
        iplFMODSetSimulationSettings =
            (PFN_iplFMODSetSimulationSettings)fmodPlugin.getFunctionPointer("iplFMODSetSimulationSettings");
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
        simulationSettings.flags = (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);
        simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
        simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        simulationSettings.maxNumRays = 2048;
        simulationSettings.maxNumOcclusionSamples = 1024;
        simulationSettings.numDiffuseSamples = 16;
        simulationSettings.maxDuration = 2.5f;
        simulationSettings.maxOrder = 1;
        simulationSettings.maxNumSources = 512;
        simulationSettings.numThreads = 15;
        simulationSettings.rayBatchSize = 16;
        simulationSettings.numVisSamples = 256;
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
        simThread = new SteamAudioSimThread(simulator, this);

        lastListenerPos = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    void AudioSystem::initialise(entt::registry& worldState)
    {
        worldState.on_destroy<AudioSource>().connect<&AudioSystem::onAudioSourceDestroy>(*this);
        g_console->registerCommand(
            [&](const char* arg) {
                if (!available)
                {
                    logErr(WELogCategoryAudio, "Audio subsystem is unavailable");
                    return;
                }

                float vol = std::atof(arg);
                if (!masterVCA->isValid())
                {
                    logErr(WELogCategoryAudio, "Master VCA handle was invalid");
                }
                else
                {
                    masterVCA->setVolume(vol);
                }
            },
            "a_setMasterVolume", "Sets the master audio volume.");

        g_console->registerCommand(
            [&](const char* arg) {
                if (!available)
                {
                    logErr(WELogCategoryAudio, "Audio subsystem is unavailable");
                    return;
                }

                float vol = std::atof(arg);
                if (!musicVCA->isValid())
                {
                    logErr(WELogCategoryAudio, "Music VCA handle was invalid");
                }
                else
                {
                    musicVCA->setVolume(vol);
                }
            },
            "a_setMusicVolume", "Sets the music audio volume.");

        g_console->registerCommand(
            [&](const char*) {
                if (!available)
                {
                    logErr(WELogCategoryAudio, "Audio subsystem is unavailable");
                    return;
                }

                updateAudioScene(worldState);
            },
            "a_forceUpdateAudioScene", "Forces an update of the Steam Audio scene.");

        g_console->registerCommand([&](const char*) { iplSceneSaveOBJ(scene, "audioScene.obj"); }, "a_dumpToObj",
                                   "Dumps the Steam Audio scene to an obj file.");
    }

    void AudioSystem::onAudioSourceDestroy(entt::registry& reg, entt::entity entity)
    {
        if (!available)
            return;
        AudioSource& as = reg.get<AudioSource>(entity);

        if (as.eventInstance)
        {
            FMCHECK(as.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
            FMCHECK(as.eventInstance->release());
        }

        if (as.phononSource)
        {
            std::unique_lock lock{simThread->commitMutex};
            sourcesToRemove.push(as.phononSource);
        }
    }

    void AudioSystem::loadMasterBanks()
    {
        if (!available)
            return;
        masterBank = loadBank("FMOD/Desktop/Master.bank");
        stringsBank = loadBank("FMOD/Desktop/Master.strings.bank");
        if (!masterBank)
            return;
        FMCHECK(studioSystem->getVCA("vca:/Master", &masterVCA));
        FMCHECK(studioSystem->getVCA("vca:/Music", &musicVCA));
    }

    FMOD_VECTOR convVec(glm::vec3 v3)
    {
        FMOD_VECTOR v{};
        v.x = -v3.x;
        v.y = v3.y;
        v.z = v3.z;
        return v;
    }

    IPLVector3 convVecSA(glm::vec3 v3)
    {
        IPLVector3 v{};
        v.x = v3.x;
        v.y = v3.y;
        v.z = v3.z;
        return v;
    }

    glm::vec3 checkV(glm::vec3 v)
    {
        if (glm::any(glm::isnan(v) || glm::isinf(v)))
        {
            logErr("invalid vec in audio");
            v = glm::vec3 { 1.0f, 0.0f, 0.0f };
        }

        return v;
    }

    ConVar a_phononUpdateRate{"a_phononUpdateRate", "0.1"};
    void AudioSystem::updateSteamAudio(entt::registry& registry, float deltaTime, glm::vec3 listenerPos,
                                       glm::quat listenerRot)
    {
        IPLSimulationFlags simFlags =
            (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);

        IPLSimulationInputs inputs{};
        inputs.flags = simFlags;

        if (glm::any(glm::isnan(listenerPos) || glm::isinf(listenerPos)))
        {
            logWarn(WELogCategoryAudio, "Listener position was NaN or infinity");
            listenerPos = glm::vec3{ 0.0f };
        }

        if (glm::any(glm::isnan(listenerRot) || glm::isinf(listenerRot)))
        {
            logWarn(WELogCategoryAudio, "Listener rotation was NaN or infinity");
            listenerRot = glm::quat { 1.0f, 0.0f, 0.0f, 0.0f };
        }

        inputs.source.right = convVecSA(checkV(listenerRot * glm::vec3(1.0f, 0.0f, 0.0f)));
        inputs.source.up = convVecSA(checkV(listenerRot * glm::vec3(0.0f, 1.0f, 0.0f)));
        inputs.source.ahead = convVecSA(checkV(listenerRot * glm::vec3(0.0f, 0.0f, 1.0f)));
        inputs.source.origin = convVecSA(checkV(listenerPos));
        inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
        inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
        inputs.reverbScale[0] = 1.0f;
        inputs.reverbScale[1] = 1.0f;
        inputs.reverbScale[2] = 1.0f;
        inputs.hybridReverbTransitionTime = 1.0f;
        inputs.hybridReverbOverlapPercent = 0.25f;
        inputs.baked = IPL_FALSE;

        iplSourceSetInputs(listenerCentricSource, simFlags, &inputs);

        for (AttachedOneshot* oneshot : attachedOneshots)
        {
            if (oneshot->phononSource == nullptr)
                continue;
            if (!registry.valid(oneshot->entity))
                continue;

            Transform& t = registry.get<Transform>(oneshot->entity);
            IPLSimulationInputs inputs{};
            inputs.flags = simFlags;
            inputs.source.right = convVecSA(t.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            inputs.source.up = convVecSA(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
            inputs.source.ahead = convVecSA(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
            inputs.source.origin = convVecSA(t.position);
            inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
            inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
            inputs.reverbScale[0] = 1.0f;
            inputs.reverbScale[1] = 1.0f;
            inputs.reverbScale[2] = 1.0f;
            inputs.hybridReverbTransitionTime = 1.0f;
            inputs.hybridReverbOverlapPercent = 0.25f;
            inputs.baked = IPL_FALSE;

            iplSourceSetInputs(oneshot->phononSource, simFlags, &inputs);
        }

        registry.view<AudioSource, Transform>().each([simFlags, this](AudioSource& as, const Transform& t) {
            if (!as.eventInstance->isValid())
                return;

            FMOD_STUDIO_PLAYBACK_STATE playbackState;
            FMCHECK(as.eventInstance->getPlaybackState(&playbackState));
            if (as.phononSource == nullptr)
                return;

            if (playbackState == FMOD_STUDIO_PLAYBACK_STOPPED)
            {
                if (as.inPhononSim)
                {
                    as.inPhononSim = false;
                    std::unique_lock lock{simThread->commitMutex};
                    needsSimCommit = true; 
                    this->sourcesToRemove.push(as.phononSource);
                }

                return;
            }

            if (!as.inPhononSim)
            {
                as.inPhononSim = true;
                std::unique_lock lock{simThread->commitMutex};
                needsSimCommit = true; 
                iplSourceRetain(as.phononSource);
                this->sourcesToAdd.push(as.phononSource);
                sourcesInSim++;
                return;
            }

            IPLSimulationInputs inputs{};
            inputs.flags = simFlags;
            inputs.directFlags = (IPLDirectSimulationFlags)(IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
            inputs.source.right = convVecSA(t.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            inputs.source.up = convVecSA(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
            inputs.source.ahead = convVecSA(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
            inputs.source.origin = convVecSA(t.position);
            inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
            inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
            inputs.occlusionType = IPL_OCCLUSIONTYPE_RAYCAST;
            inputs.numOcclusionSamples = 8;
            inputs.occlusionRadius = 0.2f;
            inputs.reverbScale[0] = 1.0f;
            inputs.reverbScale[1] = 1.0f;
            inputs.reverbScale[2] = 1.0f;
            inputs.hybridReverbTransitionTime = 1.0f;
            inputs.hybridReverbOverlapPercent = 0.25f;
            inputs.baked = IPL_FALSE;

            iplSourceSetInputs(as.phononSource, simFlags, &inputs);

            IPLSimulationOutputs outputs{};
            iplSourceGetOutputs(as.phononSource, IPL_SIMULATIONFLAGS_DIRECT, &outputs);

            FMOD::DSP* phononDsp;
            FMCHECK(findSteamAudioDSP(as.eventInstance, &phononDsp));
            if (phononDsp)
            {
                FMCHECK(phononDsp->setParameterFloat(SpatializerEffect::OCCLUSION, outputs.direct.occlusion));
            }
        });

        IPLSimulationSharedInputs sharedInputs{};
        sharedInputs.listener = inputs.source;
        sharedInputs.numRays = 1024;
        sharedInputs.numBounces = 4;
        sharedInputs.duration = 1.0f;
        sharedInputs.order = 1;
        sharedInputs.irradianceMinDistance = 1.0f;

        iplSimulatorSetSharedInputs(simulator, simFlags, &sharedInputs);
        if (!needsSimCommit)
            iplSimulatorRunDirect(simulator);

        timeSinceLastSim += deltaTime;
        if (timeSinceLastSim > a_phononUpdateRate.getFloat())
        {
            if (!simThread->isSimRunning())
            {
                simThread->runSimulation();
            }
            else
            {
                logWarn(WELogCategoryAudio, "Phonon simulation thread is falling behind");
            }
            timeSinceLastSim = 0.0f;
        }
    }

    void AudioSystem::update(entt::registry& worldState, glm::vec3 listenerPos, glm::quat listenerRot, float deltaTime)
    {
        if (!available)
            return;
        glm::vec3 movement = listenerPos - lastListenerPos;
        movement /= deltaTime;

        if (glm::any(glm::isinf(movement)) || glm::any(glm::isnan(movement)))
            movement = glm::vec3{0.0f};

        if (glm::any(glm::isnan(listenerPos)))
            listenerPos = glm::vec3{0.0f};

        FMOD_3D_ATTRIBUTES listenerAttributes{};
        listenerAttributes.forward = convVec(listenerRot * glm::vec3(0.0f, 0.0f, 1.0f));
        listenerAttributes.up = convVec(listenerRot * glm::vec3(0.0f, 1.0f, 0.0f));

        listenerAttributes.position = convVec(listenerPos);
        listenerAttributes.velocity = convVec(movement);

        updateSteamAudio(worldState, deltaTime, listenerPos, listenerRot);

        worldState.view<AudioSource, Transform>().each([](AudioSource& as, Transform& t) {
            if (as.eventInstance == nullptr)
                return;

            FMOD_3D_ATTRIBUTES sourceAttributes{};
            sourceAttributes.position = convVec(t.position);
            sourceAttributes.forward = convVec(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
            sourceAttributes.up = convVec(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));

            FMCHECK(as.eventInstance->set3DAttributes(&sourceAttributes));
        });

        FMCHECK(studioSystem->setListenerAttributes(0, &listenerAttributes, &listenerAttributes.position));

        for (AttachedOneshot* ao : attachedOneshots)
        {
            if (!worldState.valid(ao->entity) && ao->entity != entt::null)
            {
                ao->markForRemoval = true;
                FMCHECK(ao->instance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
                FMCHECK(ao->instance->release());
                continue;
            }

            FMOD_STUDIO_PLAYBACK_STATE playbackState;
            FMCHECK(ao->instance->getPlaybackState(&playbackState));

            if (playbackState == FMOD_STUDIO_PLAYBACK_STOPPED)
            {
                ao->timeSinceStop += deltaTime;

                if (ao->timeSinceStop > 0.1f)
                {
                    ao->markForRemoval = true;
                    FMCHECK(ao->instance->release());
                    continue;
                }
            }

            if (worldState.valid(ao->entity))
            {
                Transform& t = worldState.get<Transform>(ao->entity);

                FMOD_3D_ATTRIBUTES sourceAttributes{};
                sourceAttributes.position = convVec(t.position);
                sourceAttributes.forward = convVec(t.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
                sourceAttributes.up = convVec(t.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
                sourceAttributes.velocity = convVec((t.position - ao->lastPosition) / deltaTime);

                ao->lastPosition = t.position;

                FMCHECK(ao->instance->set3DAttributes(&sourceAttributes));
            }
        }

        FMCHECK(studioSystem->update());

        {
            std::unique_lock lock{simThread->commitMutex};
            attachedOneshots.erase(std::remove_if(attachedOneshots.begin(), attachedOneshots.end(),
                [this](AttachedOneshot* ao) {
                    bool marked = ao->markForRemoval;
                    if (marked)
                    {
                        if (ao->phononSource)
                        {
                            sourcesToRemove.push(ao->phononSource);
                        }
                        needsSimCommit = true;
                        delete ao;
                    }
                    return marked;
                }),
                attachedOneshots.end());
        }

        if (a_showDebugInfo.getInt())
        {
            bool keepShowing = true;

            if (ImGui::Begin(ICON_FAD_SPEAKER " Audio Debug", &keepShowing))
            {
                int currentAlloced, maxAlloced;
                FMOD::Memory_GetStats(&currentAlloced, &maxAlloced);

                ImGui::Text("AttachedOneshots: %i", (int)attachedOneshots.size());
                ImGui::Text("Sources in simulation: %i", sourcesInSim);
                if (ImGui::CollapsingHeader("Phonon"))
                {
                    ImGui::Text("Last Phonon simulation tick took %.3fms", simThread->lastStepTime());
                    ImGui::Text("Simulation running: %i", simThread->isSimRunning());
                }

                if (ImGui::CollapsingHeader("FMOD"))
                {
                    ImGui::Text("Currently allocated: %.3fMB", currentAlloced / 1024.0 / 1024.0);
                    ImGui::Text("Currently allocated: %.3fMB", maxAlloced / 1024.0 / 1024.0);
                }
            }
            ImGui::End();

            if (!keepShowing)
            {
                a_showDebugInfo.setValue("0");
            }
        }
    }

    void AudioSystem::stopEverything(entt::registry& reg)
    {
        if (!available)
            return;
        reg.view<AudioSource>().each([](AudioSource& as) { as.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE); });
    }

    void AudioSystem::playOneShotClip(AssetID id, glm::vec3 location, bool spatialise, float volume, MixerChannel)
    {
        if (!available)
            return;
        FMOD::Sound* sound;

        if (sounds.contains(id))
        {
            sound = sounds.at(id);
        }
        else
        {
            std::string path = AssetDB::idToPath(id);

            FMOD_RESULT result;
            result = system->createSound(path.c_str(), FMOD_CREATESAMPLE, nullptr, &sound);

            if (result != FMOD_OK)
            {
                logErr(WELogCategoryAudio, "Failed to create oneshot clip %s", path.c_str());
                return;
            }

            sounds.insert({id, sound});
        }

        FMOD::Channel* channel;
        FMCHECK(system->playSound(sound, nullptr, false, &channel));

        if (spatialise)
        {
            FMOD_VECTOR fmLocation = convVec(location);
            FMOD_VECTOR vel{0.0f, 0.0f, 0.0f};
            channel->set3DAttributes(&fmLocation, &vel);
        }
    }

    void AudioSystem::playOneShotEvent(const char* eventPath, glm::vec3 location, float volume)
    {
        playOneShotAttachedEvent(eventPath, location, entt::null, volume);
    }

    struct PhononEventInstanceCallbackData
    {
        IPLSource phononSource;
        glm::vec3 location;
    };

    FMOD_RESULT AudioSystem::phononEventInstanceCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type,
                                                         FMOD_STUDIO_EVENTINSTANCE* cevent, void* param)
    {
        if (type != FMOD_STUDIO_EVENT_CALLBACK_CREATED && type != FMOD_STUDIO_EVENT_CALLBACK_DESTROYED)
            return FMOD_OK;

        FMOD::Studio::EventInstance* event = (FMOD::Studio::EventInstance*)cevent;

        PhononEventInstanceCallbackData* data;
        FMCHECK(event->getUserData((void**)&data));
        FMOD::DSP* phononDsp;
        FMCHECK(findSteamAudioDSP(event, &phononDsp));

        std::lock_guard lock{AudioSystem::instance->simThread->commitMutex};
        // The FMOD plugin says here that it wants the simulation outputs.
        // However, this is wrong. The code is actually looking for the IPLSource attached
        // to the Steam Audio source!
        if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATED)
        {
            FMCHECK(phononDsp->setParameterData(
                SpatializerEffect::SIMULATION_OUTPUTS, &data->phononSource,
                sizeof(IPLSource))
            );

            FMOD_3D_ATTRIBUTES attr{};
            attr.position = convVec(data->location);
            attr.forward = convVec(glm::vec3(0.0f, 0.0f, 1.0f));
            attr.up = convVec(glm::vec3(0.0f, 1.0f, 0.0f));
            attr.velocity = convVec(glm::vec3(0.0f));
        }
        else
        {
            iplSourceRelease(&data->phononSource);
            delete data;
        }

        return FMOD_OK;
    }

    void AudioSystem::playOneShotAttachedEvent(const char* eventPath, glm::vec3 location, entt::entity attachedEntity,
                                               float volume)
    {
        if (!available)
            return;
        FMOD_RESULT result;

        bool createPhononSource = true;
        FMOD::Studio::EventDescription* desc;
        result = studioSystem->getEvent(eventPath, &desc);

        if (result != FMOD_OK)
        {
            logErr(WELogCategoryAudio, "Failed to get event %s: %s", eventPath, FMOD_ErrorString(result));
            return;
        }

        FMOD_STUDIO_USER_PROPERTY userProp;
        FMOD_RESULT propertyResult = desc->getUserProperty("UsePhononSource", &userProp);

        if (propertyResult == FMOD_ERR_EVENT_NOTFOUND)
        {
            createPhononSource = false;
        }
        else
        {
            FMCHECK(propertyResult);
        }

        int instanceCount = 0;

        FMCHECK(desc->getInstanceCount(&instanceCount));

        if (instanceCount > 100)
        {
            return;
        }

        FMOD::Studio::EventInstance* instance;

        result = desc->createInstance(&instance);
        if (result != FMOD_OK)
        {
            logErr(WELogCategoryAudio, "Failed to create instance of event %s: %s", eventPath,
                   FMOD_ErrorString(result));
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
        FMCHECK(instance->set3DAttributes(&attr));

        AttachedOneshot* attachedOneshot =
            new AttachedOneshot{.instance = instance, .entity = attachedEntity, .lastPosition = location};

        if (createPhononSource)
        {
            IPLSourceSettings sourceSettings{
                (IPLSimulationFlags)(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_DIRECT)};

            SACHECK(iplSourceCreate(simulator, &sourceSettings, &attachedOneshot->phononSource));

            PhononEventInstanceCallbackData* data = new PhononEventInstanceCallbackData();
            data->phononSource = attachedOneshot->phononSource;
            instance->setUserData(data);
            instance->setCallback(phononEventInstanceCallback, FMOD_STUDIO_EVENT_CALLBACK_CREATED | FMOD_STUDIO_EVENT_CALLBACK_DESTROYED);

            std::unique_lock lock{simThread->commitMutex};
            iplSourceRetain(attachedOneshot->phononSource);
            sourcesToAdd.push(attachedOneshot->phononSource);
            sourcesInSim++;
            needsSimCommit = true;
        }

        attachedOneshots.push_back(attachedOneshot);
    }

    void AudioSystem::shutdown(entt::registry& worldState)
    {
        if (!available)
            return;

        worldState.clear<AudioSource>();

        if (simThread)
            delete simThread;

        FMCHECK(studioSystem->release());
    }

    FMOD::Studio::Bank* AudioSystem::loadBank(const char* path)
    {
        if (!available)
            return nullptr;

        PHYSFS_getLastErrorCode();
        FMOD::Studio::Bank* bank;

        if (loadedBanks.contains(path))
            return loadedBanks.at(path);

        if (studioSystem->loadBankFile(path, FMOD_STUDIO_LOAD_BANK_NORMAL, &bank) != FMOD_OK)
        {
            return nullptr;
        }
        loadedBanks.insert({path, bank});

        return bank;
    }

    void AudioSystem::bakeProbes(entt::registry& registry)
    {
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

    void AudioSystem::saveAudioScene(entt::registry& reg, const char* path)
    {
        IPLSerializedObjectSettings settings{0};
        IPLSerializedObject serializedObject;
        SACHECK(iplSerializedObjectCreate(phononContext, &settings, &serializedObject));

        // Create the audio scene just for this
        IPLScene newScene = createScene(reg);
        iplSceneSave(newScene, serializedObject);
        iplSceneRelease(&newScene);

        PHYSFS_File* file = PHYSFS_openWrite(path);
        PHYSFS_writeBytes(file, iplSerializedObjectGetData(serializedObject),
                          iplSerializedObjectGetSize(serializedObject));
        PHYSFS_close(file);
    }

    void AudioSystem::updateAudioScene(entt::registry& reg)
    {
        ZoneScoped;

        if (scene)
            iplSceneRelease(&scene);

        std::string savedPath = "LevelData/PhononScenes/" + reg.ctx<SceneInfo>().name + ".bin";

        if (PHYSFS_exists(savedPath.c_str()))
        {
            scene = loadScene(savedPath.c_str());
        }
        else
        {
            logWarn(WELogCategoryAudio, "Scene %s doesn't have baked audio data!", reg.ctx<SceneInfo>().name.c_str());
            scene = createScene(reg);
        }

        while (simThread->isSimRunning())
        {
        }
        iplSimulatorSetScene(simulator, scene);
        needsSimCommit = true;
    }

    struct CacheableMeshInfo
    {
        std::vector<IPLTriangle> triangles;
        std::vector<IPLMaterial> materials;
        std::vector<int> materialIndices;
    };
    IPLScene AudioSystem::createScene(entt::registry& reg)
    {
        ZoneScoped;

        IPLSceneSettings sceneSettings{};
        sceneSettings.type = IPL_SCENETYPE_DEFAULT;

        IPLScene scene = nullptr;
        SACHECK(iplSceneCreate(phononContext, &sceneSettings, &scene));
        robin_hood::unordered_map<AssetID, CacheableMeshInfo> cachedMeshes;

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            if (!enumHasFlag(wo.staticFlags, StaticFlags::Audio))
                return;
            glm::mat4 tMat = t.getMatrix();

            // oh boy
            std::vector<IPLVector3> verts;

            const LoadedMesh& lm = MeshManager::loadOrGet(wo.mesh);

            if (!cachedMeshes.contains(wo.mesh))
            {
                CacheableMeshInfo cmi;
                cmi.triangles.resize(lm.indices.size() / 3);
                cmi.materialIndices.resize(cmi.triangles.size());

                uint32_t triCount = 0;

                cmi.materials.resize(32);
                for (int materialIndex = 0; materialIndex < 32; materialIndex++)
                {
                    IPLMaterial mat{{0.1f, 0.2f, 0.3f}, 0.05f, {0.1f, 0.05f, 0.03f}};

                    if (wo.presentMaterials[materialIndex])
                    {
                        auto& jMat = MaterialManager::loadOrGet(wo.materials[materialIndex]);
                        mat.scattering = jMat.value("soundScattering", mat.scattering);

                        if (jMat.contains("soundAbsorption"))
                        {
                            for (int i = 0; i < 3; i++)
                            {
                                mat.absorption[i] = jMat["soundAbsorption"][i];
                            }
                        }

                        if (jMat.contains("soundTransmission"))
                        {
                            for (int i = 0; i < 3; i++)
                            {
                                mat.transmission[i] = jMat["soundTransmission"][i];
                            }
                        }
                    }

                    cmi.materials[materialIndex] = mat;
                }

                for (int submeshIdx = 0; submeshIdx < lm.numSubmeshes; submeshIdx++)
                {
                    const SubmeshInfo& submesh = lm.submeshes[submeshIdx];

                    for (uint32_t i = 0; i < submesh.indexCount; i += 3)
                    {
                        cmi.materialIndices[triCount] = submesh.materialIndex;
                        cmi.triangles[triCount++] = {(int)lm.indices[submesh.indexOffset + i], (int)lm.indices[submesh.indexOffset + i + 1], (int)lm.indices[submesh.indexOffset + i + 2]};
                    }
                }

                cachedMeshes.insert({wo.mesh, std::move(cmi)});
            }

            CacheableMeshInfo& cmi = cachedMeshes.at(wo.mesh);

            verts.resize(lm.vertices.size());

            for (uint32_t i = 0; i < lm.vertices.size(); i++)
            {
                glm::vec3 pos = tMat * glm::vec4(lm.vertices[i].position, 1.0f);
                verts[i].x = pos.x;
                verts[i].y = pos.y;
                verts[i].z = pos.z;
            }

            IPLStaticMeshSettings settings{};
            settings.numVertices = verts.size();
            settings.numTriangles = cmi.triangles.size();
            settings.numMaterials = cmi.materials.size();
            settings.vertices = verts.data();
            settings.triangles = cmi.triangles.data();
            settings.materialIndices = cmi.materialIndices.data();
            settings.materials = cmi.materials.data();

            IPLStaticMesh mesh = nullptr;
            SACHECK(iplStaticMeshCreate(scene, &settings, &mesh));
            iplStaticMeshAdd(mesh, scene);
            iplStaticMeshRelease(&mesh);
        });
        iplSceneCommit(scene);

        return scene;
    }

    IPLScene AudioSystem::loadScene(const char* path)
    {
        PHYSFS_File* file = PHYSFS_openRead(path);

        size_t dataSize = PHYSFS_fileLength(file);
        uint8_t* buffer = new uint8_t[dataSize];

        PHYSFS_readBytes(file, buffer, dataSize);
        PHYSFS_close(file);

        IPLSerializedObjectSettings serializedObjectSettings{};
        serializedObjectSettings.data = buffer;
        serializedObjectSettings.size = dataSize;

        IPLSerializedObject serializedObject;
        SACHECK(iplSerializedObjectCreate(phononContext, &serializedObjectSettings, &serializedObject));

        IPLSceneSettings sceneSettings{};
        sceneSettings.type = IPL_SCENETYPE_DEFAULT;

        IPLScene scene = nullptr;
        SACHECK(iplSceneLoad(phononContext, &sceneSettings, serializedObject, nullptr, nullptr, &scene));

        delete[] buffer;

        return scene;
    }
}
