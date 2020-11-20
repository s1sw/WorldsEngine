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
#include "VKImGUIUtil.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "CreateModelObject.hpp"
#include "IconsFontAwesome5.h"
#include "IconsFontaudio.h"
#include "RichPresence.hpp"
#include "SplashWindow.hpp"
#include "EarlySDLUtil.hpp"

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
    bool enableOpenVR = false;
    glm::ivec2 windowSize;
    SceneInfo currentScene;

    void WorldsEngine::setupSDL() {
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER);
    }

    SDL_Window* WorldsEngine::createSDLWindow() {
        return SDL_CreateWindow("Converge", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900,
            SDL_WINDOW_VULKAN |
            SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_ALLOW_HIGHDPI |
            SDL_WINDOW_HIDDEN);
    }

    // SDL_PollEvent blocks when the window is being resized or moved,
    // so I run it on a different thread.
    // I would put it through the job system, but thanks to Windows
    // weirdness SDL_PollEvent will not work on other threads.
    // Thanks Microsoft.
    int WorldsEngine::windowThread(void* data) {
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

    void addImGuiFont(std::string fontPath, float size, ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {
        PHYSFS_File* ttfFile = PHYSFS_openRead(fontPath.c_str());
        if (ttfFile == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open font file");
            return;
        }

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

        if (config)
            memcpy(config->Name, fontPath.c_str(), fontPath.size());
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, (int)readBytes, size, config, ranges);

        PHYSFS_close(ttfFile);
    }

    void cmdLoadScene(void* obj, const char* arg) {
        if (!PHYSFS_exists(arg)) {
            logErr(WELogCategoryEngine, "Couldn't find scene %s. Make sure you included the .escn file extension.", arg);
            return;
        }
        loadScene(g_assetDB.addOrGetExisting(arg), *(entt::registry*)obj);
    }

    void cmdToggleFullscreen(void* obj, const char* arg) {
        SDL_Event evt;
        SDL_zero(evt);
        evt.type = fullscreenToggleEventId;
        SDL_PushEvent(&evt);
    }

    JobSystem* g_jobSys;

    std::unordered_map<entt::entity, physx::PxTransform> currentState;
    std::unordered_map<entt::entity, physx::PxTransform> previousState;
    extern std::function<void(entt::registry&)> onSceneLoad;

    void WorldsEngine::setupPhysfs(char* argv0) {
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
    }

    extern void loadDefaultUITheme();

    void setupUIFonts() {
        if (PHYSFS_exists("Fonts/EditorFont.ttf"))
            ImGui::GetIO().Fonts->Clear();

        addImGuiFont("Fonts/EditorFont.ttf", 18.0f);

        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.OversampleH = 1;

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAR, 17.0f, &iconConfig, iconRanges);
        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAS, 17.0f, &iconConfig, iconRanges);

        ImFontConfig iconConfig2{};
        iconConfig2.MergeMode = true;
        iconConfig2.PixelSnapH = true;
        iconConfig2.OversampleH = 1;
        iconConfig2.GlyphOffset = ImVec2(-3.0f, 5.0f);
        iconConfig2.GlyphExtraSpacing = ImVec2(-5.0f, 0.0f);

        static const ImWchar iconRangesFAD[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAD, 22.0f, &iconConfig2, iconRanges);
    }

    ConVar showDebugInfo("showDebugInfo", "0", "Shows the debug info window");
    ConVar lockSimToRefresh("sim_lockToRefresh", "0", "Instead of using a simulation timestep, run the simulation in lockstep with the rendering.");
    ConVar disableSimInterp("sim_disableInterp", "0", "Disables interpolation and uses the results of the last run simulation step.");
    ConVar simStepTime("sim_stepTime", "0.01");

    WorldsEngine::WorldsEngine(EngineInitOptions initOptions, char* argv0)
        : running{ true }
        , pauseSim{ false }
        , simAccumulator{ 0.0 }{
        ZoneScoped;
        useEventThread = initOptions.useEventThread;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        runAsEditor = initOptions.runAsEditor;
        enableOpenVR = initOptions.enableVR;

        // Initialisation Stuffs
        // =====================
        setupSDL();

        console = std::make_unique<Console>();

        auto splashWindow = createSplashWindow();
        redrawSplashWindow(splashWindow, "");

        setupPhysfs(argv0);
        redrawSplashWindow(splashWindow, "starting up");

        fullscreenToggleEventId = SDL_RegisterEvents(1);

        inputManager = std::make_unique<InputManager>(window);

        // Ensure that we have a minimum of two workers, as one worker
        // means that jobs can be missed
        g_jobSys = new JobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };

        currentScene.name = "Untitled";

        redrawSplashWindow(splashWindow, "loading assetdb");
        g_assetDB.load();

        if (useEventThread) {
            sdlEventCV = SDL_CreateCond();
            sdlEventMutex = SDL_CreateMutex();

            WindowThreadData wtd{ &running, &window };
            SDL_DetachThread(SDL_CreateThread(windowThread, "Window Thread", &wtd));
            SDL_Delay(1000);
        } else {
            window = createSDLWindow();
            if (window == nullptr) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to create window", SDL_GetError(), NULL);
            }
        }
        setWindowIcon(window);

        redrawSplashWindow(splashWindow, "initialising ui");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.IniFilename = runAsEditor ? "imgui_editor.ini" : "imgui.ini";
        // Disabling this for now as it seems to cause random freezes
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        io.Fonts->TexDesiredWidth = 512.f;

        setupUIFonts();
        loadDefaultUITheme();

        ImGui_ImplSDL2_InitForVulkan(window);

        std::vector<std::string> additionalInstanceExts;
        std::vector<std::string> additionalDeviceExts;

        if (enableOpenVR) {
            openvrInterface.init();
            uint32_t newW, newH;

            openvrInterface.getRenderResolution(&newW, &newH);
            SDL_Rect rect;
            SDL_GetDisplayUsableBounds(0, &rect);

            float scaleFac = glm::min(((float)rect.w * 0.9f) / newW, ((float)rect.h * 0.9f) / newH);

            SDL_SetWindowSize(window, newW * scaleFac, newH * scaleFac);
        }

        VrApi activeApi = VrApi::None;

        if (enableOpenVR) {
            activeApi = VrApi::OpenVR;
        }

        IVRInterface* vrInterface = &openvrInterface;

        redrawSplashWindow(splashWindow, "initialising renderer");

        RendererInitInfo initInfo{
            window,
            additionalInstanceExts, additionalDeviceExts,
            enableOpenVR, activeApi, vrInterface,
            runAsEditor, "Converge"
        };

        bool renderInitSuccess = false;
        renderer = new VKRenderer(initInfo, &renderInitSuccess);

        if (!renderInitSuccess) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to initialise renderer", window);
            return;
        }

        cam.position = glm::vec3(0.0f, 0.0f, -1.0f);

        initPhysx(registry);

        EngineInterfaces interfaces{
                .vrInterface = enableOpenVR ? &openvrInterface : nullptr,
                .renderer = renderer,
                .mainCamera = &cam,
                .inputManager = inputManager.get(),
                .engine = this
        };

        auto vkCtx = renderer->getVKCtx();
        VKImGUIUtil::createObjects(vkCtx);

        redrawSplashWindow(splashWindow, "initialising editor");

        editor = std::make_unique<Editor>(registry, interfaces);

        if (!runAsEditor)
            pauseSim = false;

        initRichPresence(interfaces);

        console->registerCommand(cmdLoadScene, "scene", "Loads a scene.", &registry);
        console->registerCommand(cmdToggleFullscreen, "toggleFullscreen", "Toggles fullscreen.", nullptr);
        console->registerCommand([&](void*, const char*) {
            runAsEditor = false;
            pauseSim = false;

            if (evtHandler)
                evtHandler->onSceneStart(registry);

            for (auto* system : systems)
                system->onSceneStart(registry);

            registry.view<AudioSource>().each([](auto ent, auto& as) {
                if (as.playOnSceneOpen) {
                    as.isPlaying = true;
                }
                });
            renderer->reloadMatsAndTextures();
            }, "play", "play.", nullptr);

        console->registerCommand([&](void*, const char*) {
            runAsEditor = true;
            pauseSim = true;
            renderer->reloadMatsAndTextures();
            }, "pauseAndEdit", "pause and edit.", nullptr);

        console->registerCommand([&](void*, const char*) {
            runAsEditor = true;
            loadScene(currentScene.id, registry);
            pauseSim = true;
            renderer->reloadMatsAndTextures();
            }, "reloadAndEdit", "reload and edit.", nullptr);

        console->registerCommand([&](void*, const char*) {
            runAsEditor = false;
            pauseSim = false;
            renderer->reloadMatsAndTextures();
            }, "unpause", "unpause and go back to play mode.", nullptr);

        console->registerCommand([&](void*, const char*) {
            renderer->reloadMatsAndTextures();
            }, "reloadContent", "Reloads materials, textures and meshes.", nullptr);

        if (runAsEditor)
            disableSimInterp.setValue("1");

        if (enableOpenVR) {
            lockSimToRefresh.setValue("1");
            disableSimInterp.setValue("1");
        }

        if (runAsEditor)
            createStartupScene();

        if (!runAsEditor && PHYSFS_exists("CommandScripts/startup.txt"))
            console->executeCommandStr("exec CommandScripts/startup");

        if (evtHandler != nullptr) {
            evtHandler->init(registry, interfaces);

            if (!runAsEditor) {
                evtHandler->onSceneStart(registry);
                for (auto* system : systems)
                    system->onSceneStart(registry);
            }
        }

        onSceneLoad = [&](entt::registry& reg) {
            if (evtHandler && !runAsEditor) {
                evtHandler->onSceneStart(reg);

                for (auto* system : systems)
                    system->onSceneStart(registry);
            }
        };

        uint32_t w, h;

        if (enableOpenVR) {
            openvrInterface.getRenderResolution(&w, &h);
        } else {
            w = 1600;
            h = 900;
        }

        RTTPassCreateInfo screenRTTCI;
        screenRTTCI.enableShadows = true;
        screenRTTCI.width = w;
        screenRTTCI.height = h;
        screenRTTCI.isVr = enableOpenVR;
        screenRTTCI.outputToScreen = true;
        screenRTTCI.useForPicking = false;
        screenRTTPass = renderer->createRTTPass(screenRTTCI);

        redrawSplashWindow(splashWindow, "initialising audio");
        audioSystem = std::make_unique<AudioSystem>();
        audioSystem->initialise(registry);

        SDL_ShowWindow(window);
        destroySplashWindow(splashWindow);
    }

    void WorldsEngine::createStartupScene() {
        AssetID grassMatId = g_assetDB.addOrGetExisting("Materials/grass.json");
        AssetID devMatId = g_assetDB.addOrGetExisting("Materials/dev.json");

        AssetID modelId = g_assetDB.addOrGetExisting("model.obj");
        AssetID monkeyId = g_assetDB.addOrGetExisting("monk.obj");
        renderer->preloadMesh(modelId);
        renderer->preloadMesh(monkeyId);
        createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, grassMatId, glm::vec3(5.0f, 1.0f, 5.0f));

        createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, devMatId);

        entt::entity dirLightEnt = registry.create();
        registry.emplace<WorldLight>(dirLightEnt, LightType::Directional);
        registry.emplace<Transform>(dirLightEnt, glm::vec3(0.0f), glm::angleAxis(glm::radians(90.01f), glm::vec3(1.0f, 0.0f, 0.0f)));
    }

    void WorldsEngine::mainLoop() {
        int frameCounter = 0;

        uint64_t last = SDL_GetPerformanceCounter();

        double deltaTime;
        double currTime = 0.0;
        double lastUpdateTime = 0.0;

        while (running) {
            uint64_t now = SDL_GetPerformanceCounter();
            bool recreateScreenRTT = false;
            if (!useEventThread) {
                SDL_Event evt;
                while (SDL_PollEvent(&evt)) {
                    if (evt.type == SDL_WINDOWEVENT) {
                        if (evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                            recreateScreenRTT = true;
                            logMsg("Recreating screen RTT pass.");
                        }
                    }

                    if (evt.type == SDL_QUIT) {
                        running = false;
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
            }

            uint64_t updateStart = SDL_GetPerformanceCounter();

            tickRichPresence();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame(window);

            ImGui::NewFrame();
            inputManager->update();

            if (runAsEditor) {
                // Create global dock space
                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->Pos);
                ImGui::SetNextWindowSize(viewport->Size);
                ImGui::SetNextWindowViewport(viewport->ID);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

                ImGui::Begin("Editor dockspace - you shouldn't be able to see this!", 0,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar);
                ImGui::PopStyleVar(3);

                ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
                ImGui::DockSpace(dockspaceId);
                ImGui::End();

                // Draw black background
                ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos, viewport->Size, ImColor(0.0f, 0.0f, 0.0f, 1.0f));
            }

            if (!renderer->isPassValid(screenRTTPass)) {
                recreateScreenRTT = true;
            } else {
                renderer->setRTTPassActive(screenRTTPass, !runAsEditor);
            }

            uint64_t deltaTicks = now - last;
            last = now;
            deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
            currTime += deltaTime;
            renderer->time = currTime;

            float interpAlpha = 1.0f;

            if (evtHandler != nullptr && !runAsEditor) {
                evtHandler->preSimUpdate(registry, deltaTime);

                for (auto* system : systems)
                    system->preSimUpdate(registry, deltaTime);
            }

            double simTime = 0.0;
            if (!pauseSim) {
                PerfTimer perfTimer;
                updateSimulation(interpAlpha, deltaTime);
                simTime = perfTimer.stopGetMs();
            }

            if (evtHandler != nullptr && !runAsEditor) {
                evtHandler->update(registry, deltaTime, interpAlpha);

                for (auto* system : systems)
                    system->update(registry, deltaTime, interpAlpha);
            }

            SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

            editor->setActive(runAsEditor);
            editor->update((float)deltaTime);

            if (inputManager->keyPressed(SDL_SCANCODE_RCTRL, true)) {
                SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
            }

            if (inputManager->keyPressed(SDL_SCANCODE_F3, true)) {
                renderer->recreateSwapchain();
            }

            if (inputManager->keyPressed(SDL_SCANCODE_F11, true)) {
                SDL_Event evt;
                SDL_zero(evt);
                evt.type = fullscreenToggleEventId;
                SDL_PushEvent(&evt);
            }

            uint64_t updateEnd = SDL_GetPerformanceCounter();

            uint64_t updateLength = updateEnd - updateStart;
            double updateTime = updateLength / (double)SDL_GetPerformanceFrequency();

            DebugTimeInfo dti;
            dti.deltaTime = deltaTime;
            dti.frameCounter = frameCounter;
            dti.lastUpdateTime = lastUpdateTime;
            dti.updateTime = updateTime;
            dti.simTime = simTime;
            drawDebugInfoWindow(dti);

            if (enableOpenVR) {
                auto pVRSystem = vr::VRSystem();

                float fSecondsSinceLastVsync;
                pVRSystem->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, NULL);

                float fDisplayFrequency = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
                float fFrameDuration = 1.f / fDisplayFrequency;
                float fVsyncToPhotons = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

                float fPredictedSecondsFromNow = fFrameDuration - fSecondsSinceLastVsync + fVsyncToPhotons;

                // Not sure why we predict an extra frame here, but it feels like crap without it
                renderer->setVRPredictAmount(fPredictedSecondsFromNow + fFrameDuration);
            }

            audioSystem->update(registry, cam.position, cam.rotation);

            console->drawWindow();

            ImGui::Render();

            if (useEventThread) {
                SDL_LockMutex(sdlEventMutex);
                SDL_CondSignal(sdlEventCV);
                SDL_UnlockMutex(sdlEventMutex);
            }

            glm::vec3 camPos = cam.position;

            /*registry.sort<ProceduralObject>([&](entt::entity a, entt::entity b) {
                auto& aTransform = registry.get<Transform>(a);
                auto& bTransform = registry.get<Transform>(b);
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position);
                }, entt::insertion_sort{});

            registry.sort<WorldObject>([&](entt::entity a, entt::entity b) {
                auto& aTransform = registry.get<Transform>(a);
                auto& bTransform = registry.get<Transform>(b);
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position) || registry.has<UseWireframe>(a);
                }, entt::insertion_sort{});*/

            renderer->frame(cam, registry);
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            g_jobSys->completeFrameJobs();
            frameCounter++;

            inputManager->endFrame();

            lastUpdateTime = updateTime;

            if (recreateScreenRTT) {
                int newWidth;
                int newHeight;

                SDL_GetWindowSize(window, &newWidth, &newHeight);

                if (enableOpenVR) {
                    uint32_t w, h;
                    openvrInterface.getRenderResolution(&w, &h);
                    newWidth = w;
                    newHeight = h;
                }

                RTTPassCreateInfo screenRTTCI;
                screenRTTCI.enableShadows = true;
                screenRTTCI.width = newWidth;
                screenRTTCI.height = newHeight;
                screenRTTCI.isVr = enableOpenVR;
                screenRTTCI.outputToScreen = true;
                screenRTTCI.useForPicking = false;
                screenRTTPass = renderer->createRTTPass(screenRTTCI);
            }
        }
    }

    void WorldsEngine::drawDebugInfoWindow(DebugTimeInfo timeInfo) {
        if (showDebugInfo.getInt()) {
            bool open = true;
            if (ImGui::Begin("Info", &open)) {
                static float historicalFrametimes[128] = { 0.0f };
                static int historicalFrametimeIdx = 0;

                historicalFrametimes[historicalFrametimeIdx] = timeInfo.deltaTime * 1000.0;
                historicalFrametimeIdx++;
                if (historicalFrametimeIdx >= 128) {
                    historicalFrametimeIdx = 0;
                }

                if (ImGui::CollapsingHeader(ICON_FA_CLOCK u8" Performance")) {
                    ImGui::PlotLines("Historical Frametimes", historicalFrametimes, 128, historicalFrametimeIdx, nullptr, 0.0f, 20.0f, ImVec2(0.0f, 125.0f));
                    ImGui::Text("Frametime: %.3fms", timeInfo.deltaTime * 1000.0);
                    ImGui::Text("Update time: %.3fms", timeInfo.updateTime * 1000.0);
                    ImGui::Text("Physics time: %.3fms", timeInfo.simTime);
                    ImGui::Text("Update time without physics: %.3fms", (timeInfo.updateTime * 1000.0) - timeInfo.simTime);
                    ImGui::Text("Framerate: %.1ffps", 1.0 / timeInfo.deltaTime);
                }

                if (ImGui::CollapsingHeader(ICON_FA_BARS u8" Misc")) {
                    ImGui::Text("Frame: %i", timeInfo.frameCounter);
                    ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);

                    if (ImGui::Button("Unload Unused Assets")) {
                        renderer->unloadUnusedMaterials(registry);
                    }

                    if (ImGui::Button("Reload Materials and Textures")) {
                        renderer->reloadMatsAndTextures();
                    }
                }

                if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" Render Stats")) {
                    ImGui::Text("Draw calls: %i", renderer->getDebugStats().numDrawCalls);
                    ImGui::Text("Frustum culled objects: %i", renderer->getDebugStats().numCulledObjs);
                    ImGui::Text("GPU memory usage: %.3fMB", (double)renderer->getDebugStats().vramUsage / 1024.0 / 1024.0);
                    ImGui::Text("Active RTT passes: %i", renderer->getDebugStats().numRTTPasses);
                    ImGui::Text("Time spent in renderer: %.3fms", (timeInfo.deltaTime - timeInfo.lastUpdateTime) * 1000.0);
                    ImGui::Text("GPU render time: %.3fms", renderer->getLastRenderTime() / 1000.0f / 1000.0f);
                    ImGui::Text("V-Sync status: %s", renderer->getVsync() ? "On" : "Off");

                    size_t numLights = registry.view<WorldLight>().size();
                    size_t worldObjects = registry.view<WorldObject>().size();

                    ImGui::Text("%u light(s) / %u world object(s)", numLights, worldObjects);
                }
            }
            ImGui::End();

            if (!open) {
                showDebugInfo.setValue("0");
            }
        }
    }

    void WorldsEngine::updateSimulation(float& interpAlpha, double deltaTime) {
        if (lockSimToRefresh.getInt() || disableSimInterp.getInt()) {
            registry.view<DynamicPhysicsActor, Transform>().each([](auto ent, DynamicPhysicsActor& dpa, Transform& transform) {
                auto curr = dpa.actor->getGlobalPose();

                if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation)) {
                    physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                    dpa.actor->setGlobalPose(pt);
                }
                });
        }

        registry.view<PhysicsActor, Transform>().each([](auto ent, PhysicsActor& pa, Transform& transform) {
            auto curr = pa.actor->getGlobalPose();
            if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation)) {
                physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                pa.actor->setGlobalPose(pt);
            }
            });

        if (!lockSimToRefresh.getInt()) {
            simAccumulator += deltaTime;

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

                if (evtHandler != nullptr && !runAsEditor) {
                    evtHandler->simulate(registry, simStepTime.getFloat());

                    for (auto* system : systems)
                        system->simulate(registry, simStepTime.getFloat());
                }

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

            if (evtHandler != nullptr && !runAsEditor) {
                evtHandler->simulate(registry, deltaTime);

                for (auto* system : systems)
                    system->simulate(registry, deltaTime);
            }

            registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity ent, DynamicPhysicsActor& dpa, Transform& transform) {
                transform.position = px2glm(dpa.actor->getGlobalPose().p);
                transform.rotation = px2glm(dpa.actor->getGlobalPose().q);
                });
        }
    }

    void WorldsEngine::addSystem(ISystem* system) {
        systems.push_back(system);
    }

    WorldsEngine::~WorldsEngine() {
        for (auto* system : systems) {
            delete system;
        }

        if (evtHandler != nullptr && !runAsEditor)
            evtHandler->shutdown(registry);

        auto vkCtx = renderer->getVKCtx();
        registry.clear();
        shutdownRichPresence();
        VKImGUIUtil::destroyObjects(vkCtx);
        delete renderer;
        shutdownPhysx();
        PHYSFS_deinit();
        if (useEventThread) SDL_CondSignal(sdlEventCV);
        logMsg("Quitting SDL.");
        SDL_Quit();
    }
}