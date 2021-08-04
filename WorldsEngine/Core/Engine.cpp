#include <SDL.h>
#include <atomic>
#include <iostream>
#include <thread>
#include "JobSystem.hpp"
#include "Engine.hpp"
#include "../ImGui/imgui.h"
#include <physfs.h>
#include "../ImGui/imgui_impl_sdl.h"
#include "../ImGui/imgui_impl_vulkan.h"
#include <entt/entt.hpp>
#include "Scripting/NetVM.hpp"
#include "Transform.hpp"
#include "../Physics/Physics.hpp"
#include "../Input/Input.hpp"
#include "../Physics/PhysicsActor.hpp"
#include <glm/gtx/norm.hpp>
#include <physx/PxQueryReport.h>
#include "tracy/Tracy.hpp"
#include "../Editor/Editor.hpp"
#include "../VR/OpenVRInterface.hpp"
#include "../Core/Log.hpp"
#include "../Audio/Audio.hpp"
#include <stb_image.h>
#include "../Core/Console.hpp"
#include "../Serialization/SceneSerialization.hpp"
#include "../Render/Render.hpp"
#include "../Util/TimingUtil.hpp"
#include "../Util/VKImGUIUtil.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../ImGui/imgui_internal.h"
#include "../Util/CreateModelObject.hpp"
#include "../Libs/IconsFontAwesome5.h"
#include "../Libs/IconsFontaudio.h"
#include "../Util/RichPresence.hpp"
#include "SplashWindow.hpp"
#include "EarlySDLUtil.hpp"
#include "vk_mem_alloc.h"
#include "../Render/ShaderCache.hpp"
#include "readerwriterqueue.h"
#include "../ComponentMeta/ComponentMetadata.hpp"
#include "../Util/EnumUtil.hpp"
#undef min
#undef max

namespace worlds {
    uint32_t fullscreenToggleEventId;
    uint32_t showWindowEventId;

    bool useEventThread = false;
    int workerThreadOverride = -1;
    bool enableOpenVR = false;
    glm::ivec2 windowSize;

    // event thread sync
    moodycamel::ReaderWriterQueue<SDL_Event> evts;

    void WorldsEngine::setupSDL() {
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER);
        SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
        SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
        SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
        SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);
    }

    SDL_Window* WorldsEngine::createSDLWindow() {
        return SDL_CreateWindow("Loading...",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1600, 900,
            SDL_WINDOW_VULKAN |
            SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_ALLOW_HIGHDPI |
            SDL_WINDOW_HIDDEN
        );
    }

    bool fullscreen = false;
    volatile bool windowCreated = false;

    // SDL_PollEvent blocks when the window is being resized or moved,
    // so I run it on a different thread.
    // I would put it through the job system, but thanks to Windows
    // weirdness SDL_PollEvent will not work on other threads.
    // Thanks Microsoft.
    int WorldsEngine::windowThread(void* data) {
        WorldsEngine* _this = (WorldsEngine*)data;

        _this->window = createSDLWindow();
        if (_this->window == nullptr) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "err", SDL_GetError(), NULL);
        }

        if (_this->runAsEditor)
            setWindowIcon(_this->window, "icon_engine.png");
        else
            setWindowIcon(_this->window);

        windowCreated = true;
        while (_this->running) {
            SDL_Event evt;

            if (SDL_WaitEvent(&evt)) {
                if (evt.type == SDL_QUIT) {
                    _this->running = false;
                    break;
                } else if (evt.type == fullscreenToggleEventId) {
                    if (fullscreen) {
                        SDL_SetWindowResizable(_this->window, SDL_TRUE);
                        SDL_SetWindowBordered(_this->window, SDL_TRUE);
                        SDL_SetWindowPosition(_this->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        SDL_SetWindowSize(_this->window, 1600, 900);
                    } else {
                        SDL_SetWindowResizable(_this->window, SDL_FALSE);
                        SDL_SetWindowBordered(_this->window, SDL_FALSE);
                        SDL_SetWindowPosition(_this->window, 0, 0);
                        SDL_DisplayMode dm;
                        SDL_GetDesktopDisplayMode(0, &dm);
                        SDL_SetWindowSize(_this->window, dm.w, dm.h);
                    }
                    fullscreen = !fullscreen;
                } else if (evt.type == showWindowEventId) {
                    uint32_t flags = SDL_GetWindowFlags(_this->window);

                    if (flags & SDL_WINDOW_HIDDEN)
                        SDL_ShowWindow(_this->window);
                    else
                        SDL_HideWindow(_this->window);
                    logMsg("window state toggled");
                } else {
                    evts.emplace(evt);
                }
            }
        }

        logMsg("window thread exiting");
        // SDL requires threads to return an int
        return 0;
    }

    void addImGuiFont(std::string fontPath, float size, ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {
        PHYSFS_File* ttfFile = PHYSFS_openRead(fontPath.c_str());
        if (ttfFile == nullptr) {
            logWarn("Couldn't open font file");
            return;
        }

        auto fileLength = PHYSFS_fileLength(ttfFile);

        if (fileLength == -1) {
            PHYSFS_close(ttfFile);
            logWarn("Couldn't determine size of editor font file");
            return;
        }

        void* buf = std::malloc(fileLength);
        auto readBytes = PHYSFS_readBytes(ttfFile, buf, fileLength);

        if (readBytes != fileLength) {
            PHYSFS_close(ttfFile);
            logWarn("Failed to read full TTF file");
            return;
        }

        ImFontConfig defaultConfig{};

        if (config) {
            memcpy(config->Name, fontPath.c_str(), fontPath.size());
        } else { 
            for (int i = 0; i < fontPath.size(); i++) {
                defaultConfig.Name[i] = fontPath[i];
            }
            config = &defaultConfig;
        }

        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, (int)readBytes, size, config, ranges);

        PHYSFS_close(ttfFile);
    }

    void cmdToggleFullscreen(void*, const char*) {
        SDL_Event evt;
        SDL_zero(evt);
        evt.type = fullscreenToggleEventId;
        SDL_PushEvent(&evt);
    }

    JobSystem* g_jobSys;

    std::unordered_map<entt::entity, physx::PxTransform> currentState;
    std::unordered_map<entt::entity, physx::PxTransform> previousState;

    void WorldsEngine::setupPhysfs(char* argv0) {
        const char* dataFolder = "GameData";
        const char* engineDataFolder = "EngineData";
        const char* dataSrc = "SrcData";
        const char* basePath = SDL_GetBasePath();

        std::string dataStr(basePath);
        dataStr += dataFolder;

        std::string engineDataStr(basePath);
        engineDataStr += engineDataFolder;

        std::string srcDataStr(basePath);
        srcDataStr += dataSrc;

        SDL_free((void*)basePath);

        PHYSFS_init(argv0);
        logMsg("Mounting %s", dataStr.c_str());
        PHYSFS_mount(dataStr.c_str(), "/", 0);
        logMsg("Mounting %s", engineDataStr.c_str());
        PHYSFS_mount(engineDataStr.c_str(), "/", 0);
        logMsg("Mounting %s", srcDataStr.c_str());
        PHYSFS_mount(srcDataStr.c_str(), "/SrcData", 0);
        PHYSFS_setWriteDir(dataStr.c_str());

        PHYSFS_permitSymbolicLinks(1);
    }

    extern void loadDefaultUITheme();

    void setupUIFonts() {
        if (PHYSFS_exists("Fonts/EditorFont.ttf"))
            ImGui::GetIO().Fonts->Clear();

        addImGuiFont("Fonts/EditorFont.ttf", 20.0f);

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
        iconConfig2.OversampleH = 2;
        iconConfig2.GlyphOffset = ImVec2(-3.0f, 5.0f);
        iconConfig2.GlyphExtraSpacing = ImVec2(-5.0f, 0.0f);

        static const ImWchar iconRangesFAD[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAD, 22.0f, &iconConfig2, iconRangesFAD);
    }

    ConVar showDebugInfo { "showDebugInfo", "0", "Shows the debug info window" };
    ConVar lockSimToRefresh {
        "sim_lockToRefresh", "0",
        "Instead of using a simulation timestep, run the simulation in lockstep with the rendering." };
    ConVar disableSimInterp {
        "sim_disableInterp", "0",
        "Disables interpolation and uses the results of the last run simulation step."
    };
    ConVar simStepTime{ "sim_stepTime", "0.01" };

    WorldsEngine::WorldsEngine(EngineInitOptions initOptions, char* argv0)
        : pauseSim{ false }
        , running{ true }
        , simAccumulator{ 0.0 } {
        ZoneScoped;
        useEventThread = initOptions.useEventThread;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        runAsEditor = initOptions.runAsEditor;
        enableOpenVR = initOptions.enableVR;
        dedicatedServer = initOptions.dedicatedServer;

        // Initialisation Stuffs
        // =====================
        setupSDL();

        console = std::make_unique<Console>(dedicatedServer);

        worlds::SplashWindow* splashWindow = nullptr;

        if (!dedicatedServer) {
            splashWindow = new SplashWindow(!runAsEditor);
        }

        setupPhysfs(argv0);
        if (!dedicatedServer && runAsEditor) {
            splashWindow->changeOverlay("starting up");
        }

        fullscreenToggleEventId = SDL_RegisterEvents(1);
        showWindowEventId = SDL_RegisterEvents(1);

        // Ensure that we have a minimum of two workers, as one worker
        // means that jobs can be missed
        g_jobSys = new JobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };

        currentScene.name = "Untitled";
        currentScene.id = ~0u;

        if (!dedicatedServer && runAsEditor)
            splashWindow->changeOverlay("loading assetdb");

        AssetDB::load();
        registry.set<SceneSettings>(AssetDB::pathToId("Cubemaps/Miramar/miramar.json"));

        if (!dedicatedServer) {
            if (useEventThread) {
                SDL_DetachThread(SDL_CreateThread(windowThread, "Window Thread", this));
                while(!windowCreated){}
            } else {
                window = createSDLWindow();
                if (window == nullptr) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                            "Failed to create window", SDL_GetError(), NULL);
                }

                if (runAsEditor)
                    setWindowIcon(window, "icon_engine.png");
                else
                    setWindowIcon(window);
            }

            SDL_SetWindowTitle(window, initOptions.gameName);

            if (runAsEditor)
                splashWindow->changeOverlay("initialising ui");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = runAsEditor ? "imgui_editor.ini" : "imgui.ini";
        io.Fonts->TexDesiredWidth = 512;

        if (!dedicatedServer) {
            ImGui_ImplSDL2_InitForVulkan(window);
            setupUIFonts();
            loadDefaultUITheme();
        }

        std::vector<std::string> additionalInstanceExts;
        std::vector<std::string> additionalDeviceExts;

        if (enableOpenVR) {
            openvrInterface = std::make_unique<OpenVRInterface>();
            openvrInterface->init();

            if (!runAsEditor) {
                uint32_t newW, newH;

                openvrInterface->getRenderResolution(&newW, &newH);
                SDL_Rect rect;
                SDL_GetDisplayUsableBounds(0, &rect);

                float scaleFac = glm::min(((float)rect.w * 0.9f) / newW, ((float)rect.h * 0.9f) / newH);

                SDL_SetWindowSize(window,
                        (uint32_t)(newW * scaleFac),
                        (uint32_t)(newH * scaleFac));
            }
        }

        VrApi activeApi = VrApi::None;

        if (enableOpenVR) {
            activeApi = VrApi::OpenVR;
        }

        IVRInterface* vrInterface = openvrInterface.get();

        if (!dedicatedServer && runAsEditor)
            splashWindow->changeOverlay("initialising renderer");

        if (!dedicatedServer) {
            RendererInitInfo initInfo{
                window,
                additionalInstanceExts, additionalDeviceExts,
                enableOpenVR, activeApi, vrInterface,
                runAsEditor, initOptions.gameName
            };

            bool renderInitSuccess = false;
            renderer = std::make_unique<VKRenderer>(initInfo, &renderInitSuccess);

            if (!renderInitSuccess) {
                fatalErr("Failed to initialise renderer");
                return;
            }
        }

        cam.position = glm::vec3(0.0f, 0.0f, -1.0f);

        initPhysx(registry);
        inputManager = std::make_unique<InputManager>(window);

        EngineInterfaces interfaces{
            .vrInterface = enableOpenVR ? openvrInterface.get() : nullptr,
            .renderer = renderer.get(),
            .mainCamera = &cam,
            .inputManager = inputManager.get(),
            .engine = this
        };

        if (!dedicatedServer) {
            VKImGUIUtil::createObjects(renderer->getHandles());
        }

        scriptEngine = std::make_unique<DotNetScriptEngine>(registry, interfaces);
        interfaces.scriptEngine = scriptEngine.get();
        if (!dedicatedServer && runAsEditor) {
            splashWindow->changeOverlay("initialising editor");
            editor = std::make_unique<Editor>(registry, interfaces);
        } else {
            ComponentMetadataManager::setupLookup();
        }

        if (!scriptEngine->initialise(editor.get()))
            fatalErr("Failed to initialise .net");

        if (!runAsEditor)
            pauseSim = false;

        if (!dedicatedServer)
            initRichPresence(interfaces);

        console->registerCommand([&](void*, const char* arg) {
            if (!PHYSFS_exists(arg)) {
                logErr(WELogCategoryEngine, "Couldn't find scene %s. Make sure you included the .escn file extension.", arg);
                return;
            }
            loadScene(AssetDB::pathToId(arg));
        }, "scene", "Loads a scene.", nullptr);

        console->registerCommand([&](void*, const char* arg) {
            uint32_t id = (uint32_t)std::atoll(arg);
            if (AssetDB::exists(id))
                logMsg("Asset %u: %s", id, AssetDB::idToPath(id).c_str());
            else
                logErr("Nonexistent asset");
        }, "adb_lookupID", "Looks up an asset ID.", nullptr);

        console->registerCommand([&](void*, const char* arg) {
            timeScale = atof(arg);
        }, "setTimeScale", "Sets the current time scale.", nullptr);

        if (!dedicatedServer) {
            console->registerCommand(cmdToggleFullscreen, "toggleFullscreen", "Toggles fullscreen.", nullptr);

            if (runAsEditor) {
                console->registerCommand([&](void*, const char*) {
                    editor->active = false;
                    pauseSim = false;

                    if (evtHandler)
                        evtHandler->onSceneStart(registry);

                    for (auto* system : systems)
                        system->onSceneStart(registry);

                    scriptEngine->onSceneStart();

                    registry.view<AudioSource>().each([](auto, auto& as) {
                        if (as.playOnSceneOpen) {
                            as.isPlaying = true;
                        }
                    });
                    inputManager->lockMouse(true);
                    if (!enableOpenVR)
                        console->executeCommandStr("sim_lockToRefresh 0");
                }, "play", "play.");

                console->registerCommand([&](void*, const char*) {
                    editor->active = true;
                    pauseSim = true;
                    inputManager->lockMouse(false);
                    if (!enableOpenVR)
                        console->executeCommandStr("sim_lockToRefresh 1");
                }, "pauseAndEdit", "pause and edit.");

                console->registerCommand([&](void*, const char*) {
                    editor->active = true;
                    if (currentScene.id != ~0u)
                        loadScene(currentScene.id);
                    pauseSim = true;
                    inputManager->lockMouse(false);
                    if (!enableOpenVR)
                        console->executeCommandStr("sim_lockToRefresh 1");
                }, "reloadAndEdit", "reload and edit.");

                console->registerCommand([&](void*, const char*) {
                    editor->active = false;
                    pauseSim = false;
                    inputManager->lockMouse(true);
                    if (!enableOpenVR)
                        console->executeCommandStr("sim_lockToRefresh 0");
                }, "unpause", "unpause and go back to play mode.");
            }

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::All);
            }, "reloadContent", "Reloads all content.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Textures);
            }, "reloadTextures", "Reloads textures.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Materials);
            }, "reloadMaterials", "Reloads materials.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Meshes);
            }, "reloadMeshes", "Reloads meshes.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Cubemaps);
            }, "reloadCubemaps", "Reloads cubemaps.", nullptr);
        }

        console->registerCommand([&](void*, const char*) {
            running = false;
        }, "exit", "Shuts down the engine.", nullptr);

        console->registerCommand([this](void*, const char* arg) {
            std::string argS = arg;
            size_t xPos = argS.find("x");

            if (xPos == std::string::npos) {
                logErr("Invalid window size (specify like 1280x720)");
                return;
            }
            int width = std::stoi(argS.substr(0, xPos));
            int height = std::stoi(argS.substr(xPos + 1));
            SDL_SetWindowSize(window, width, height);
        }, "setWindowSize", "Sets size of the window.", nullptr);

        if (enableOpenVR) {
            lockSimToRefresh.setValue("1");
        }

        if (runAsEditor) {
            //disableSimInterp.setValue("1");
            lockSimToRefresh.setValue("1");
            createStartupScene();
        }

        if (!dedicatedServer) {
            if (runAsEditor)
                splashWindow->changeOverlay("initialising audio");

            audioSystem = std::make_unique<AudioSystem>();
            audioSystem->initialise(registry);
        }

        if (!runAsEditor && PHYSFS_exists("CommandScripts/startup.txt"))
            console->executeCommandStr("exec CommandScripts/startup");

        if (dedicatedServer)
            console->executeCommandStr("exec CommandScripts/server_startup");

        if (evtHandler != nullptr) {
            evtHandler->init(registry, interfaces);

            if (!runAsEditor) {
                evtHandler->onSceneStart(registry);

                for (auto* system : systems)
                    system->onSceneStart(registry);

                scriptEngine->onSceneStart();
            }
        }


        if (!dedicatedServer) {
            uint32_t w = 1600;
            uint32_t h = 900;

            if (enableOpenVR) {
                openvrInterface->getRenderResolution(&w, &h);
            }

            RTTPassCreateInfo screenRTTCI {
                .width = w,
                .height = h,
                .isVr = enableOpenVR,
                .useForPicking = false,
                .enableShadows = true,
                .outputToScreen = true
            };

            screenRTTPass = renderer->createRTTPass(screenRTTCI);

            logMsg("deleting splashWindow");
            delete splashWindow;
            logMsg("splashWIndow deleted");

            if (useEventThread) {
                SDL_Event evt;
                evt.type = showWindowEventId;
                SDL_PushEvent(&evt);
            } else {
                SDL_ShowWindow(window);
            }
        }


        if (dedicatedServer) {
            // do a lil' dance to make dear imgui happy
            auto& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(800.0f, 600.0f);
            unsigned char* outPixels;
            int w, h;
            io.Fonts->GetTexDataAsAlpha8(&outPixels, &w, &h);

            // and then we just throw away its hard work :(
            // TODO: find a better way to do this without wasting so much
            std::free(outPixels);
            io.IniFilename = nullptr;
        }
    }

    void WorldsEngine::createStartupScene() {
        registry.clear();
        AssetID grassMatId = AssetDB::pathToId("Materials/grass.json");
        AssetID devMatId = AssetDB::pathToId("Materials/dev.json");

        AssetID modelId = AssetDB::pathToId("Models/cube.wmdl");
        AssetID monkeyId = AssetDB::pathToId("Models/monkey.wmdl");
        renderer->preloadMesh(modelId);
        renderer->preloadMesh(monkeyId);
        createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, grassMatId, glm::vec3(5.0f, 1.0f, 5.0f));

        createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, devMatId);

        entt::entity dirLightEnt = registry.create();
        registry.emplace<WorldLight>(dirLightEnt, LightType::Directional);
        registry.emplace<Transform>(dirLightEnt, glm::vec3(0.0f), glm::angleAxis(glm::radians(90.01f), glm::vec3(1.0f, 0.0f, 0.0f)));
        currentScene.name = "Untitled";
        currentScene.id = ~0u;
    }

    void WorldsEngine::processEvents() {
        if (!useEventThread) {
            SDL_Event evt;
            while (SDL_PollEvent(&evt)) {
                if (evt.type == SDL_QUIT) {
                    running = false;
                    break;
                }

                if (evt.type == fullscreenToggleEventId) {
                    auto currentWindowFlags = SDL_GetWindowFlags(window);
                    if (enumHasFlag<Uint32>(currentWindowFlags, SDL_WINDOW_FULLSCREEN_DESKTOP))
                        SDL_SetWindowFullscreen(window, 0);
                    else
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                }

                inputManager->processEvent(evt);

                if (ImGui::GetCurrentContext())
                    ImGui_ImplSDL2_ProcessEvent(&evt);
            }
        } else {
            SDL_Event evt;
            while (evts.try_dequeue(evt)) {
                inputManager->processEvent(evt);

                if (ImGui::GetCurrentContext())
                    ImGui_ImplSDL2_ProcessEvent(&evt);
            }
        }
    }

    void WorldsEngine::mainLoop() {
        int frameCounter = 0;

        uint64_t last = SDL_GetPerformanceCounter();

        double deltaTime;
        double lastUpdateTime = 0.0;

        while (running) {
            uint64_t now = SDL_GetPerformanceCounter();
            bool recreateScreenRTT = false;
            uint64_t updateStart = SDL_GetPerformanceCounter();

            processEvents();

            if (!dedicatedServer) {
                tickRichPresence();

                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL2_NewFrame(window);
            }

            ImGui::NewFrame();
            inputManager->update();

            if (openvrInterface)
                openvrInterface->updateInput();

            if (!dedicatedServer) {
                if (!screenRTTPass->isValid) {
                    recreateScreenRTT = true;
                } else {
                    screenRTTPass->active = !runAsEditor || !editor->active;
                }
            }

            uint64_t deltaTicks = now - last;
            last = now;
            deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
            gameTime += deltaTime;
            if (!dedicatedServer)
                renderer->time = gameTime;

            float interpAlpha = 1.0f;

            if (evtHandler != nullptr && (!runAsEditor || !editor->active)) {
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

            if (evtHandler != nullptr && !(runAsEditor && editor->active)) {
                for (auto* system : systems)
                    system->update(registry, deltaTime * timeScale, interpAlpha);

                evtHandler->update(registry, deltaTime * timeScale, interpAlpha);
                scriptEngine->onUpdate(deltaTime * timeScale);
            }

            if (!dedicatedServer) {
                SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

                if (runAsEditor) {
                    editor->update((float)deltaTime);
                }
            }

            if (inputManager->keyPressed(SDL_SCANCODE_RCTRL, true)) {
                SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
            }

            if (inputManager->keyPressed(SDL_SCANCODE_F3, true)) {
                ShaderCache::clear();
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

            DebugTimeInfo dti {
                .deltaTime = deltaTime,
                .updateTime = updateTime,
                .simTime = simTime,
                .lastUpdateTime = lastUpdateTime,
                .frameCounter = frameCounter
            };

            drawDebugInfoWindow(dti);

            static ConVar drawFPS { "drawFPS", "0", "Draws a simple FPS counter in the corner of the screen." };
            if (drawFPS.getInt()) {
                auto drawList = ImGui::GetForegroundDrawList();
                char buf[128];
                snprintf(buf, 128, "%.1f fps (%.3fms)", 1.0f / dti.deltaTime, dti.deltaTime * 1000.0f);
                auto bgSize = ImGui::CalcTextSize(buf);
                auto pos = ImGui::GetMainViewport()->Pos;
                drawList->AddRectFilled(pos, pos + bgSize, ImColor(0.0f, 0.0f, 0.0f, 0.5f));
                ImColor col{1.0f, 1.0f, 1.0f};

                if (dti.deltaTime > 0.02)
                    col = ImColor{1.0f, 0.0f, 0.0f};
                else if (dti.deltaTime > 0.017)
                    col = ImColor{0.75f, 0.75f, 0.0f};
                drawList->AddText(pos, ImColor(1.0f, 1.0f, 1.0f), buf);
            }

            if (enableOpenVR) {
                auto vrSys = vr::VRSystem();

                float secondsSinceLastVsync;
                vrSys->GetTimeSinceLastVsync(&secondsSinceLastVsync, NULL);

                float hmdFrequency = vrSys->GetFloatTrackedDeviceProperty(
                    vr::k_unTrackedDeviceIndex_Hmd,
                    vr::Prop_DisplayFrequency_Float
                );

                float frameDuration = 1.f / hmdFrequency;
                float vsyncToPhotons = vrSys->GetFloatTrackedDeviceProperty(
                    vr::k_unTrackedDeviceIndex_Hmd,
                    vr::Prop_SecondsFromVsyncToPhotons_Float
                );

                float predictAmount = frameDuration - secondsSinceLastVsync + vsyncToPhotons;

                // Not sure why we predict an extra frame here, but it feels like crap without it
                renderer->setVRPredictAmount(predictAmount + frameDuration);
            }

            if (glm::any(glm::isnan(cam.position))) {
                cam.position = glm::vec3{ 0.0f };
                logWarn("cam.position was NaN!");
            }

            if (!dedicatedServer) {
                glm::vec3 alPos = cam.position;
                glm::quat alRot = cam.rotation;

                auto view = registry.view<AudioListenerOverride, Transform>();
                if (view.size_hint() > 0) {
                    auto overrideEnt = view.front();
                    auto overrideT = registry.get<Transform>(overrideEnt);
                    alPos = overrideT.position;
                    alRot = overrideT.rotation;
                }
                audioSystem->update(registry, alPos, alRot);
            }

            console->drawWindow();

            glm::vec3 camPos = cam.position;

            if (!dedicatedServer) {
                registry.sort<ProceduralObject>([&](entt::entity a, entt::entity b) {
                    auto& aTransform = registry.get<Transform>(a);
                    auto& bTransform = registry.get<Transform>(b);
                    return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position);
                });

                registry.sort<WorldObject>([&](entt::entity a, entt::entity b) {
                    auto& aTransform = registry.get<Transform>(a);
                    auto& bTransform = registry.get<Transform>(b);
                    return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position);
                });

                renderer->frame(cam, registry);

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }

            g_jobSys->completeFrameJobs();
            frameCounter++;

            inputManager->endFrame();

            for (auto& e : nextFrameKillList) {
                if (registry.valid(e))
                    registry.destroy(e);
            }

            nextFrameKillList.clear();

            if (sceneLoadQueued) {
                sceneLoadQueued = false;
                PHYSFS_File* file = AssetDB::openAssetFileRead(queuedSceneID);
                SceneLoader::loadScene(file, registry);

                if (renderer) {
                    renderer->uploadSceneAssets(registry);
                    renderer->unloadUnusedMaterials(registry);
                }

                currentScene.name = std::filesystem::path(AssetDB::idToPath(queuedSceneID)).stem().string();
                currentScene.id = queuedSceneID;

                if (evtHandler && (!runAsEditor || !editor->active)) {
                    evtHandler->onSceneStart(registry);

                    for (auto* system : systems)
                        system->onSceneStart(registry);

                    scriptEngine->onSceneStart();
                }

                registry.view<AudioSource>().each([](auto, auto& as) {
                    if (as.playOnSceneOpen) {
                        as.isPlaying = true;
                    }
                });
            }

            lastUpdateTime = updateTime;

            if (recreateScreenRTT) {
                int newWidth, newHeight;

                SDL_GetWindowSize(window, &newWidth, &newHeight);

                if (enableOpenVR) {
                    uint32_t w, h;
                    openvrInterface->getRenderResolution(&w, &h);
                    newWidth = w;
                    newHeight = h;
                }

                renderer->destroyRTTPass(screenRTTPass);

                RTTPassCreateInfo screenRTTCI {
                    .width = static_cast<uint32_t>(newWidth),
                    .height = static_cast<uint32_t>(newHeight),
                    .isVr = enableOpenVR,
                    .useForPicking = false,
                    .enableShadows = true,
                    .outputToScreen = true,
                };

                screenRTTPass = renderer->createRTTPass(screenRTTCI);
            }

            uint64_t postUpdate = SDL_GetPerformanceCounter();
            double completeUpdateTime = (postUpdate - now) / (double)SDL_GetPerformanceFrequency();

            if (dedicatedServer) {
                double waitTime = simStepTime.getFloat() - completeUpdateTime;
                if (waitTime > 0.0)
                    SDL_Delay(waitTime * 1000);
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
                    ImGui::PlotLines("Historical Frametimes", historicalFrametimes, 128, historicalFrametimeIdx, nullptr,
                        0.0f, 20.0f, ImVec2(0.0f, 125.0f));
                    ImGui::Text("Frametime: %.3fms", timeInfo.deltaTime * 1000.0);
                    ImGui::Text("Update time: %.3fms", timeInfo.updateTime * 1000.0);
                    ImGui::Text("Physics time: %.3fms", timeInfo.simTime);
                    ImGui::Text("Update time without physics: %.3fms", (timeInfo.updateTime * 1000.0) - timeInfo.simTime);
                    ImGui::Text("Framerate: %.1ffps", 1.0 / timeInfo.deltaTime);
                }

                if (ImGui::CollapsingHeader(ICON_FA_BARS u8" Misc")) {
                    ImGui::Text("Frame: %i", timeInfo.frameCounter);
                    ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);
                    ImGui::Text("Current scene: %s (%u)", currentScene.name.c_str(), currentScene.id);

                    if (ImGui::Button("Unload Unused Assets")) {
                        renderer->unloadUnusedMaterials(registry);
                    }

                    if (ImGui::Button("Reload Materials and Textures")) {
                        renderer->reloadContent(ReloadFlags::Textures | ReloadFlags::Materials);
                    }
                }

                if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" Render Stats")) {
                    const auto& dbgStats = renderer->getDebugStats();
                    auto vkCtx = renderer->getHandles();

                    VmaBudget budget[16];
                    vmaGetBudget(vkCtx->allocator, budget);
                    const VkPhysicalDeviceMemoryProperties* memProps;
                    vmaGetMemoryProperties(vkCtx->allocator, &memProps);

                    size_t totalUsage = 0;
                    size_t totalBlockBytes = 0;
                    size_t totalBudget = 0;

                    for (uint32_t i = 0; i < memProps->memoryHeapCount; i++) {
                        if (budget->budget == UINT64_MAX) continue;
                        totalUsage += budget->allocationBytes;
                        totalBlockBytes += budget->blockBytes;
                        totalBudget += budget->budget;
                    }

                    ImGui::Text("Draw calls: %i", dbgStats.numDrawCalls);
                    ImGui::Text("%i pipeline switches", dbgStats.numPipelineSwitches);
                    ImGui::Text("Frustum culled objects: %i", dbgStats.numCulledObjs);
                    ImGui::Text("GPU memory usage: %.3fMB (%.3fMB allocated, %.3fMB available)",
                        (double)totalUsage / 1024.0 / 1024.0,
                        (double)totalBlockBytes / 1024.0 / 1024.0,
                        (double)totalBudget / 1024.0 / 1024.0);
                    ImGui::Text("Active RTT passes: %i/%i", dbgStats.numActiveRTTPasses, dbgStats.numRTTPasses);
                    ImGui::Text("Time spent in renderer: %.3fms", (timeInfo.deltaTime - timeInfo.lastUpdateTime) * 1000.0);
                    ImGui::Text("GPU render time: %.3fms", renderer->getLastRenderTime() / 1000.0f / 1000.0f);
                    ImGui::Text("V-Sync status: %s", renderer->getVsync() ? "On" : "Off");
                    ImGui::Text("Triangles: %u", dbgStats.numTriangles);

                    size_t numLights = registry.view<WorldLight>().size();
                    size_t worldObjects = registry.view<WorldObject>().size();

                    ImGui::Text("%zu light(s) / %zu world object(s)", numLights, worldObjects);
                }

                if (ImGui::CollapsingHeader(ICON_FA_MEMORY u8" Memory Stats")) {
                    auto vkCtx = renderer->getHandles();

                    VmaBudget budget[16];
                    vmaGetBudget(vkCtx->allocator, budget);
                    const VkPhysicalDeviceMemoryProperties* memProps;
                    vmaGetMemoryProperties(vkCtx->allocator, &memProps);

                    for (uint32_t i = 0; i < memProps->memoryHeapCount; i++) {
                        if (ImGui::TreeNode((void*)(uintptr_t)i, "Heap %u", i)) {
                            ImGui::Text("Available: %.3fMB", budget[i].budget / 1024.0 / 1024.0);
                            ImGui::Text("Used: %.3fMB", budget[i].allocationBytes / 1024.0 / 1024.0);
                            ImGui::Text("Actually allocated: %.3fMB", budget[i].usage / 1024.0 / 1024.0);

                            ImGui::TreePop();
                        }
                    }
                }

                if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Stats")) {
                    uint32_t nDynamic = g_scene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC);
                    uint32_t nStatic = g_scene->getNbActors(physx::PxActorTypeFlag::eRIGID_STATIC);
                    uint32_t nTotal = nDynamic + nStatic;

                    ImGui::Text("%u dynamic actors, %u static actors (%u total)", nDynamic, nStatic, nTotal);
                    uint32_t nConstraints = g_scene->getNbConstraints();
                    ImGui::Text("%u constraints", nConstraints);
                    uint32_t nShapes = g_physics->getNbShapes();
                    ImGui::Text("%u shapes", nShapes);
                }
            }
            ImGui::End();

            if (!open) {
                showDebugInfo.setValue("0");
            }
        }
    }

    void WorldsEngine::doSimStep(float deltaTime) {
        stepSimulation(deltaTime);

        if (evtHandler != nullptr && !(runAsEditor && editor->active)) {
            evtHandler->simulate(registry, deltaTime);

            for (auto* system : systems)
                system->simulate(registry, deltaTime);
        }

        if (!runAsEditor || !editor->active) {
            scriptEngine->onSimulate(deltaTime);
        }
    }

    void WorldsEngine::updateSimulation(float& interpAlpha, double deltaTime) {
        ZoneScoped;
        if (lockSimToRefresh.getInt() || disableSimInterp.getInt()) {
            registry.view<DynamicPhysicsActor, Transform>().each([](DynamicPhysicsActor& dpa, Transform& transform) {
                auto curr = dpa.actor->getGlobalPose();

                if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation)) {
                    physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                    dpa.actor->setGlobalPose(pt);
                }
            });
        }

        registry.view<PhysicsActor, Transform>().each([](PhysicsActor& pa, Transform& transform) {
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
                ZoneScopedN("Simulation step");
                previousState = currentState;
                simAccumulator -= simStepTime.getFloat();

                PerfTimer timer;

                doSimStep(simStepTime.getFloat() * timeScale);

                double realTime = timer.stopGetMs() / 1000.0;

                // avoid spiral of death if simulation is taking too long
                if (realTime > simStepTime.getFloat())
                    simAccumulator = 0.0;

                registry.view<DynamicPhysicsActor>().each([&](auto ent, DynamicPhysicsActor& dpa) {
                    currentState[ent] = dpa.actor->getGlobalPose();
                });
            }

            float alpha = simAccumulator / simStepTime.getFloat();

            if (disableSimInterp.getInt())
                alpha = 1.0f;

            registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity ent, DynamicPhysicsActor&, Transform& transform) {
                transform.position = glm::mix(px2glm(previousState[ent].p), px2glm(currentState[ent].p), (float)alpha);
                transform.rotation = glm::slerp(px2glm(previousState[ent].q), px2glm(currentState[ent].q), (float)alpha);
            });
            interpAlpha = alpha;
        } else if (deltaTime < 0.05f) {
            if (enableOpenVR) {
                float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
                float fFrameDuration = 1.f / fDisplayFrequency;
                if (deltaTime >= fFrameDuration * 2.0f) {
                    doSimStep(deltaTime * 0.5f);
                    doSimStep(deltaTime * 0.5f);
                } else {
                    doSimStep(deltaTime);
                }
            } else {
                doSimStep(deltaTime);
            }

            registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity, DynamicPhysicsActor& dpa, Transform& transform) {
                transform.position = px2glm(dpa.actor->getGlobalPose().p);
                transform.rotation = px2glm(dpa.actor->getGlobalPose().q);
            });
        }
    }

    void WorldsEngine::loadScene(AssetID scene) {
        sceneLoadQueued = true;
        queuedSceneID = scene;
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

        registry.clear();

        if (!dedicatedServer) {
            shutdownRichPresence();

            auto vkCtx = renderer->getHandles();
            VKImGUIUtil::destroyObjects(vkCtx);
        }

        shutdownPhysx();
        PHYSFS_deinit();
        logMsg("Quitting SDL.");
        SDL_Quit();
    }
}
