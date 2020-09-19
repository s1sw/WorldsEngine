#include "PCH.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "JobSystem.hpp"
#include <iostream>
#include <thread>
#include "Engine.hpp"
#include "imgui.h"
#include <physfs.h>
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include "cxxopts.hpp"
#include <entt/entt.hpp>
#include "Transform.hpp"
#include "Physics.hpp"
#include "Input.hpp"
#include "PhysicsActor.hpp"
#include <execution>
#include <glm/gtx/norm.hpp>
#include <physx/PxQueryReport.h>
#include "tracy/Tracy.hpp"
#include "XRInterface.hpp"
#include "Editor.hpp"
#include "OpenVRInterface.hpp"
#include "Log.hpp"
#include "Audio.hpp"
#include <discord_rpc.h>
#include <stb_image.h>
#include "Console.hpp"
#include "SceneSerialization.hpp"
#include "Render.hpp"
#include "TimingUtil.hpp"

namespace worlds {
    AssetDB g_assetDB;

#undef min
#undef max

    SDL_cond* sdlEventCV;
    SDL_mutex* sdlEventMutex;

    struct WindowThreadData {
        bool* runningPtr;
        SDL_Window** windowVarPtr;
    };

    SDL_Window* window = nullptr;
    uint32_t fullscreenToggleEventId;

    bool useEventThread = false;
    int workerThreadOverride = -1;
    bool enableXR = false;
    bool enableOpenVR = false;
    bool runAsEditor = true;
    bool pauseSim = false;
    glm::ivec2 windowSize;
    SceneInfo currentScene;
    IGameEventHandler* evtHandler;

    SDL_TimerID presenceUpdateTimer;
    void onDiscordReady(const DiscordUser* user) {
        logMsg("Rich presence ready for %s", user->username);

        presenceUpdateTimer = SDL_AddTimer(1000, [](uint32_t interval, void*) {
            std::string state = ((runAsEditor ? "Editing " : "On ") + currentScene.name);
            DiscordRichPresence richPresence;
            memset(&richPresence, 0, sizeof(richPresence));
            richPresence.state = state.c_str();
            richPresence.largeImageKey = "logo";
            richPresence.largeImageText = "Private";
            if (!runAsEditor) {
                richPresence.partyId = "1365";
                richPresence.partyMax = 256;
                richPresence.partySize = 1;
                richPresence.joinSecret = "someone";
            }

            Discord_UpdatePresence(&richPresence);
            return interval;
            }, nullptr);
    }

    void initRichPresence() {
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        handlers.ready = onDiscordReady;
        Discord_Initialize("742075252028211310", &handlers, 0, nullptr);
    }

    void tickRichPresence() {
        Discord_RunCallbacks();
    }

    void shutdownRichPresence() {
        Discord_Shutdown();
    }

    void setupSDL() {
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER);
    }

    SDL_Window* createSDLWindow() {
        return SDL_CreateWindow("Converge", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    }

    // SDL_PollEvent blocks when the window is being resized or moved,
    // so I run it on a different thread.
    // I would put it through the job system, but thanks to Windows
    // weirdness SDL_PollEvent will not work on other threads.
    // Thanks Microsoft.
    int windowThread(void* data) {
        WindowThreadData* wtd = reinterpret_cast<WindowThreadData*>(data);

        bool* running = wtd->runningPtr;

        window = createSDLWindow();
        if (window == nullptr) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "err", SDL_GetError(), NULL);
        }

        while (*running) {
            SDL_Event evt;
            while (SDL_PollEvent(&evt)) {
                if (evt.type == SDL_QUIT) {
                    *running = false;
                    break;
                }

                if (evt.type == fullscreenToggleEventId) {
                    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_SetWindowFullscreen(window, 0);
                    } else {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }

                if (ImGui::GetCurrentContext())
                    ImGui_ImplSDL2_ProcessEvent(&evt);
            }

            SDL_LockMutex(sdlEventMutex);
            SDL_CondWait(sdlEventCV, sdlEventMutex);
            SDL_UnlockMutex(sdlEventMutex);
        }

        // SDL requires threads to return an int
        return 0;
    }

    entt::entity createModelObject(entt::registry& reg, glm::vec3 position, glm::quat rotation, AssetID meshId, AssetID materialId, glm::vec3 scale = glm::vec3(1.0f), glm::vec4 texScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)) {
        if (glm::length(rotation) == 0.0f) {
            rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        auto ent = reg.create();
        auto& transform = reg.emplace<Transform>(ent, position, rotation);
        transform.scale = scale;
        auto& worldObject = reg.emplace<WorldObject>(ent, 0, meshId);
        worldObject.texScaleOffset = texScaleOffset;
        worldObject.materials[0] = materialId;
        return ent;
    }

    void loadEditorFont() {
        ImGui::GetIO().Fonts->Clear();
        PHYSFS_File* ttfFile = PHYSFS_openRead("Fonts/EditorFont.ttf");
        size_t fileLength = PHYSFS_fileLength(ttfFile);

        if (fileLength == -1) {
            PHYSFS_close(ttfFile);
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't determine size of editor font file");
            return;
        }

        void* buf = std::malloc(fileLength);
        size_t readBytes = PHYSFS_readBytes(ttfFile, buf, fileLength);

        if (readBytes != fileLength) {
            PHYSFS_close(ttfFile);
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to read full TTF file");
            return;
        }

        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, (int)readBytes, 18.0f);

        //std::free(buf);
        PHYSFS_close(ttfFile);
    }

    void setWindowIcon() {
        SDL_Surface* surf;

        PHYSFS_File* f = PHYSFS_openRead("icon.png");
        int64_t fileLength = PHYSFS_fileLength(f);
        char* buf = (char*)std::malloc(fileLength);
        PHYSFS_readBytes(f, buf, fileLength);
        PHYSFS_close(f);

        int width, height, channels;
        unsigned char* imgDat = stbi_load_from_memory((stbi_uc*)buf, (int)fileLength, &width, &height, &channels, STBI_rgb_alpha);

        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        int shift = 0;
        rmask = 0xff000000 >> shift;
        gmask = 0x00ff0000 >> shift;
        bmask = 0x0000ff00 >> shift;
        amask = 0x000000ff;
#else // little endian, like x86
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
#endif
        surf = SDL_CreateRGBSurfaceFrom((void*)imgDat, width, height, 32, 4 * width, rmask, gmask, bmask, amask);
        SDL_SetWindowIcon(window, surf);
    }

    void cmdLoadScene(void* obj, const char* arg) {
        loadScene(g_assetDB.addOrGetExisting(arg), *(entt::registry*)obj);
    }

    void cmdToggleFullscreen(void* obj, const char* arg) {
        SDL_Event evt;
        SDL_zero(evt);
        evt.type = fullscreenToggleEventId;
        SDL_PushEvent(&evt);
    }

    JobSystem* g_jobSys;
    double simAccumulator = 0.0;

    std::unordered_map<entt::entity, physx::PxTransform> currentState;
    std::unordered_map<entt::entity, physx::PxTransform> previousState;
    void engine(char* argv0) {
        ZoneScoped;
        // Initialisation Stuffs
        // =====================
        setupSDL();
        Console console;

        fullscreenToggleEventId = SDL_RegisterEvents(1);

        InputManager inputManager{ window };

        // Ensure that we have a minimum of two workers, as one worker
        // means that jobs can be missed
        JobSystem jobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };
        g_jobSys = &jobSystem;

        const char* dataFolder = "EEData";
        const char* dataSrcFolder = "EEDataSrc";
        const char* basePath = SDL_GetBasePath();

        std::string dataStr(basePath);
        dataStr += dataFolder;

        std::string dataSrcStr(basePath);
        dataSrcStr += dataSrcFolder;

        SDL_free((void*)basePath);

        PHYSFS_init(argv0);
        logMsg("Mounting %s", dataStr.c_str());
        PHYSFS_mount(dataStr.c_str(), "/", 0);
        logMsg("Mounting source %s", dataSrcStr.c_str());
        PHYSFS_mount(dataSrcStr.c_str(), "/source", 1);
        PHYSFS_setWriteDir(dataStr.c_str());

        currentScene.name = "";

        g_assetDB.load();

        bool running = true;

        if (useEventThread) {
            sdlEventCV = SDL_CreateCond();
            sdlEventMutex = SDL_CreateMutex();

            WindowThreadData wtd{ &running, &window };
            SDL_DetachThread(SDL_CreateThread(windowThread, "Window Thread", &wtd));
            SDL_Delay(1000);
        } else {
            window = createSDLWindow();
            if (window == nullptr) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "err", SDL_GetError(), NULL);
            }
        }

        setWindowIcon();
        setupAudio();

        int frameCounter = 0;

        uint64_t last = SDL_GetPerformanceCounter();

        double deltaTime;
        double currTime = 0.0;
        double lastUpdateTime = 0.0;
        bool renderInitSuccess = false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        // Disabling this for now as it seems to cause random freezes
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

        if (PHYSFS_exists("Fonts/EditorFont.ttf")) {
            loadEditorFont();
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "EditorFont doesn't exist.");
        }
        ImGui_ImplSDL2_InitForVulkan(window);

        std::vector<std::string> additionalInstanceExts;
        std::vector<std::string> additionalDeviceExts;

        XRInterface xrInterface;
        OpenVRInterface openvrInterface;

        if (enableXR) {
            xrInterface.initXR();
            auto xrInstExts = xrInterface.getVulkanInstanceExtensions();
            auto xrDevExts = xrInterface.getVulkanDeviceExtensions();

            additionalInstanceExts.insert(additionalInstanceExts.begin(), xrInstExts.begin(), xrInstExts.end());
            additionalDeviceExts.insert(additionalDeviceExts.begin(), xrDevExts.begin(), xrDevExts.end());
        } else if (enableOpenVR) {
            openvrInterface.init();
            uint32_t newW, newH;

            openvrInterface.getRenderResolution(&newW, &newH);
            SDL_Rect rect;
            SDL_GetDisplayUsableBounds(0, &rect);

            float scaleFac = glm::min(((float)rect.w * 0.9f) / newW, ((float)rect.h * 0.9f) / newH);

            SDL_SetWindowSize(window, newW * scaleFac, newH * scaleFac);
        }

        VrApi activeApi = VrApi::None;

        if (enableXR) {
            activeApi = VrApi::OpenXR;
        } else if (enableOpenVR) {
            activeApi = VrApi::OpenVR;
        }

        IVRInterface* vrInterface = enableXR ? (IVRInterface*)&xrInterface : &openvrInterface;

        RendererInitInfo initInfo{ window, additionalInstanceExts, additionalDeviceExts, enableXR || enableOpenVR, activeApi, vrInterface, runAsEditor };
        VKRenderer* renderer = new VKRenderer(initInfo, &renderInitSuccess);

        if (!renderInitSuccess) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to initialise renderer", window);
            return;
        }

        Camera cam{};
        cam.position = glm::vec3(0.0f, 0.0f, -1.0f);
        const Uint8* state = SDL_GetKeyboardState(NULL);
        Uint8 lastState[SDL_NUM_SCANCODES];

        entt::registry registry;

        AssetID grassMatId = g_assetDB.addOrGetExisting("Materials/grass.json");
        AssetID devMatId = g_assetDB.addOrGetExisting("Materials/dev.json");

        AssetID modelId = g_assetDB.addOrGetExisting("model.obj");
        AssetID monkeyId = g_assetDB.addOrGetExisting("monk.obj");
        renderer->preloadMesh(modelId);
        renderer->preloadMesh(monkeyId);
        entt::entity boxEnt = createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, grassMatId, glm::vec3(5.0f, 1.0f, 5.0f));

        createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, devMatId);

        entt::entity dirLightEnt = registry.create();
        registry.emplace<WorldLight>(dirLightEnt, LightType::Directional);
        registry.emplace<Transform>(dirLightEnt, glm::vec3(0.0f), glm::angleAxis(glm::radians(90.01f), glm::vec3(1.0f, 0.0f, 0.0f)));

        AssetID lHandId = g_assetDB.addOrGetExisting("lhand.obj");

        renderer->preloadMesh(lHandId);

        initPhysx(registry);

        //SDL_SetRelativeMouseMode(SDL_TRUE);
        std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

        Editor editor(registry, inputManager, cam);

        if (!runAsEditor)
            pauseSim = false;

        float vrPredictAmount = 0.0f;

        initRichPresence();

        console.registerCommand(cmdLoadScene, "scene", "Loads a scene.", &registry);
        console.registerCommand(cmdToggleFullscreen, "toggleFullscreen", "Toggles fullscreen.", nullptr);

        ConVar showDebugInfo("showDebugInfo", "0", "Shows the debug info window");
        ConVar lockSimToRefresh("sim_lockToRefresh", "0", "Instead of using a simulation timestep, run the simulation in lockstep with the rendering.");
        ConVar disableSimInterp("sim_disableInterp", "0", "Disables interpolation and uses the results of the last run simulation step.");
        ConVar simStepTime("sim_stepTime", "0.01");

        if (!runAsEditor && PHYSFS_exists("CommandScripts/startup.txt"))
            console.executeCommandStr("exec CommandScripts/startup");

        if (evtHandler != nullptr && !runAsEditor) {
            EngineInterfaces interfaces{
                .vrInterface = enableOpenVR ? &openvrInterface : nullptr,
                .renderer = renderer,
                .mainCamera = &cam,
                .inputManager = &inputManager
            };
            evtHandler->init(registry, interfaces);
        }

        bool firstFrame = true;

        /*PHYSFS_File* file = g_assetDB.openAssetFileRead(g_assetDB.addOrGetExisting("torso.vtf"));
        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        loadVtfTexture(fileVec.data(), fileLen, g_assetDB.addOrGetExisting("torso.vtf"));*/

        while (running) {
            uint64_t now = SDL_GetPerformanceCounter();
            if (!useEventThread) {
                SDL_Event evt;
                while (SDL_PollEvent(&evt)) {
                    if (evt.type == SDL_QUIT) {
                        running = false;
                        break;
                    }

                    if (evt.type == fullscreenToggleEventId) {
                        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN) {
                            SDL_SetWindowFullscreen(window, 0);
                        } else {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                        }
                    }

                    if (ImGui::GetCurrentContext())
                        ImGui_ImplSDL2_ProcessEvent(&evt);
                }
            }

            uint64_t updateStart = SDL_GetPerformanceCounter();

            tickRichPresence();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame(window);

            ImGui::NewFrame();
            inputManager.update();

            uint64_t deltaTicks = now - last;
            last = now;
            deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
            currTime += deltaTime;
            renderer->time = currTime;

            float interpAlpha = 1.0f;

            if (evtHandler != nullptr && !runAsEditor)
                evtHandler->preSimUpdate(registry, deltaTime);

            double simTime = 0.0;
            if (!pauseSim) {
                PerfTimer perfTimer;
                /*registry.view<DynamicPhysicsActor, Transform>().each([](auto ent, DynamicPhysicsActor& dpa, Transform& transform) {
                    auto curr = dpa.actor->getGlobalPose();
                    if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation)) {
                        physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                        dpa.actor->setGlobalPose(pt);
                    }
                    });*/

                    //if (runAsEditor) 
                {
                    // Moving static physics actors in the editor is allowed
                    registry.view<PhysicsActor, Transform>().each([](auto ent, PhysicsActor& pa, Transform& transform) {
                        auto curr = pa.actor->getGlobalPose();
                        if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation)) {
                            physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                            pa.actor->setGlobalPose(pt);
                        }
                        });
                }

                simAccumulator += deltaTime;

                if (!lockSimToRefresh.getInt()) {
                    if (registry.view<DynamicPhysicsActor>().size() != currentState.size()) {
                        currentState.clear();
                        previousState.clear();

                        currentState.reserve(registry.view<DynamicPhysicsActor>().size());
                        previousState.reserve(registry.view<DynamicPhysicsActor>().size());

                        registry.view<DynamicPhysicsActor>().each([&](auto ent, DynamicPhysicsActor& dpa) {
                            auto startTf = dpa.actor->getGlobalPose();
                            currentState.insert({ ent, startTf });
                            previousState.insert({ ent, startTf });
                            });
                    }

                    while (simAccumulator >= simStepTime.getFloat()) {
                        previousState = currentState;
                        stepSimulation(simStepTime.getFloat());
                        simAccumulator -= simStepTime.getFloat();

                        if (evtHandler != nullptr && !runAsEditor)
                            evtHandler->simulate(registry, simStepTime.getFloat());

                        registry.view<DynamicPhysicsActor>().each([&](auto ent, DynamicPhysicsActor& dpa) {
                            currentState[ent] = dpa.actor->getGlobalPose();
                            });
                    }

                    float alpha = simAccumulator / simStepTime.getFloat();

                    if (disableSimInterp.getInt())
                        alpha = 1.0f;

                    registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity ent, DynamicPhysicsActor& dpa, Transform& transform) {
                        transform.position = glm::mix(px2glm(previousState[ent].p), px2glm(currentState[ent].p), (float)alpha);
                        transform.rotation = glm::slerp(px2glm(previousState[ent].q), px2glm(currentState[ent].q), (float)alpha);
                        });
                    interpAlpha = alpha;
                } else {
                    stepSimulation(deltaTime);

                    if (evtHandler != nullptr && !runAsEditor)
                        evtHandler->simulate(registry, deltaTime);

                    registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity ent, DynamicPhysicsActor& dpa, Transform& transform) {
                        transform.position = px2glm(dpa.actor->getGlobalPose().p);
                        transform.rotation = px2glm(dpa.actor->getGlobalPose().q);
                        });
                }

                simTime = perfTimer.stopGetMs();
            }

            if (evtHandler != nullptr && !runAsEditor)
                evtHandler->update(registry, deltaTime, interpAlpha);

            SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

            if (runAsEditor)
                editor.update((float)deltaTime);

            if (state[SDL_SCANCODE_RCTRL] && !lastState[SDL_SCANCODE_RCTRL]) {
                SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
            }

            if (state[SDL_SCANCODE_F3] && !lastState[SDL_SCANCODE_F3]) {
                renderer->recreateSwapchain();
            }

            if (state[SDL_SCANCODE_F11] && !lastState[SDL_SCANCODE_F11]) {
                SDL_Event evt;
                SDL_zero(evt);
                evt.type = fullscreenToggleEventId;
                SDL_PushEvent(&evt);
            }

            if (inputManager.mouseButtonPressed(MouseButton::Left)) {
                renderer->requestEntityPick();
            }

            entt::entity picked;
            if (renderer->getPickedEnt(&picked)) {
                if ((uint32_t)picked == UINT32_MAX)
                    picked = entt::null;

                editor.select(picked);
            }

            uint64_t updateEnd = SDL_GetPerformanceCounter();

            uint64_t updateLength = updateEnd - updateStart;
            double updateTime = updateLength / (double)SDL_GetPerformanceFrequency();

            if (showDebugInfo.getInt()) {
                if (ImGui::Begin("Info")) {
                    ImGui::Text("Frametime: %.3fms", deltaTime * 1000.0);
                    ImGui::Text("Update time: %.3fms", updateTime * 1000.0);
                    ImGui::Text("Physics time: %.3fms", simTime);
                    ImGui::Text("Update time without physics: %.3fms", (updateTime * 1000.0) - simTime);
                    ImGui::Text("Time spent in renderer: %.3fms", (deltaTime - lastUpdateTime) * 1000.0);
                    ImGui::Text("Framerate: %.3ffps", 1.0 / deltaTime);
                    ImGui::Text("GPU render time: %.3fms", renderer->getLastRenderTime() / 1000.0f / 1000.0f);
                    ImGui::Text("Frame: %i", frameCounter);
                    ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);

                    if (ImGui::Button("Unload Unused Assets")) {
                        renderer->unloadUnusedMaterials(registry);
                    }

                    if (ImGui::Button("Reload Materials and Textures")) {
                        renderer->reloadMatsAndTextures();
                    }
                }
                ImGui::End();
            }

            if (enableOpenVR) {
                auto pVRSystem = vr::VRSystem();
                float vsyncToPhoton = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                    vr::Prop_SecondsFromVsyncToPhotons_Float);

                float fSecondsSinceLastVsync;
                pVRSystem->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, NULL);

                float fDisplayFrequency = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
                float fFrameDuration = 1.f / fDisplayFrequency;
                float fVsyncToPhotons = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

                float fPredictedSecondsFromNow = fFrameDuration - fSecondsSinceLastVsync + fVsyncToPhotons;

                // Not sure why we predict an extra frame here, but it feels like crap without it
                renderer->setVRPredictAmount(fPredictedSecondsFromNow + fFrameDuration);
            }

            std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

            console.drawWindow();

            ImGui::Render();

            if (useEventThread) {
                SDL_LockMutex(sdlEventMutex);
                SDL_CondSignal(sdlEventCV);
                SDL_UnlockMutex(sdlEventMutex);
            }

            glm::vec3 camPos = cam.position;

            registry.sort<ProceduralObject>([&registry, &camPos](entt::entity a, entt::entity b) {
                auto& aTransform = registry.get<Transform>(a);
                auto& bTransform = registry.get<Transform>(b);
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, aTransform.position);
                }, entt::insertion_sort{});

            registry.sort<WorldObject>([&registry, &camPos](entt::entity a, entt::entity b) {
                auto& aTransform = registry.get<Transform>(a);
                auto& bTransform = registry.get<Transform>(b);
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, aTransform.position) || registry.has<UseWireframe>(a);
                }, entt::insertion_sort{});

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            renderer->frame(cam, registry);
            jobSystem.completeFrameJobs();
            frameCounter++;


            inputManager.endFrame();

            lastUpdateTime = updateTime;
        }

        if (evtHandler != nullptr && !runAsEditor)
            evtHandler->shutdown(registry);
        auto procObjView = registry.view<ProceduralObject>();
        registry.clear();
        shutdownRichPresence();
        delete renderer;
        shutdownPhysx();
        PHYSFS_deinit();
        if (useEventThread) SDL_CondSignal(sdlEventCV);
        logMsg("Quitting SDL.");
        SDL_Quit();
    }

    void initEngine(EngineInitOptions initOptions, char* argv0) {
        useEventThread = initOptions.useEventThread;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        engine(argv0);
    }
}