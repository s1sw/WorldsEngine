#include "Engine.hpp"
#include <Audio/Audio.hpp>
#include <Core/Console.hpp>
#include <Core/EarlySDLUtil.hpp>
#include <Core/Fatal.hpp>
#include <Core/HierarchyUtil.hpp>
#include <Core/ISystem.hpp>
#include <Core/Log.hpp>
#include <Core/MaterialManager.hpp>
#include <Core/NameComponent.hpp>
#ifdef __linux__
#include <Core/SplashScreenImpls/SplashScreenX11.hpp>
#else
#include <Core/SplashScreenImpls/SplashScreenWin32.hpp>
#endif
#include <Core/TaskScheduler.hpp>
#include <Core/Transform.hpp>
#include <Core/Window.hpp>
#include <ComponentMeta/ComponentMetadata.hpp>
#include <Editor/Editor.hpp>
#include <entt/entt.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_freetype.h>
#include <ImGui/imgui_impl_sdl.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui_internal.h>
#include <Input/Input.hpp>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <physfs.h>
#include <Physics/Physics.hpp>
#include <Physics/PhysicsActor.hpp>
#include <Render/Render.hpp>
#include <Render/RenderInternal.hpp>
#include <SDL.h>
#include <Scripting/NetVM.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Tracy.hpp>
#include <Util/CreateModelObject.hpp>
#include <Util/TimingUtil.hpp>
#include <VR/OpenXRInterface.hpp>
#include <filesystem>
#include <Core/EngineInternal.hpp>

#ifdef BUILD_EDITOR
#define EDITORONLY(expr) expr
#else
#define EDITORONLY(expr)
#endif

#ifdef CHECK_NEW_DELETE
robin_hood::unordered_map<void*, size_t> allocatedPtrs;
size_t allocatedMem = 0;
size_t liveAllocations = 0;

void* operator new(size_t count)
{
    void* ptr = malloc(count);
#ifdef TRACY_ENABLE
    TracyAlloc(ptr, count);
#endif
    allocatedPtrs.insert({ptr, count});
    allocatedMem += count;
    liveAllocations++;
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    if (ptr == nullptr)
        return;
    if (!allocatedPtrs.contains(ptr))
    {
        fatalErr("Deleted non-existent pointer!");
    }
#ifdef TRACY_ENABLE
    TracyFree(ptr);
#endif
    free(ptr);
    allocatedMem -= allocatedPtrs[ptr];
    allocatedPtrs.erase(ptr);
    liveAllocations--;
}
#endif

namespace worlds
{
    glm::ivec2 windowSize;
    enki::TaskScheduler g_taskSched;

    void WorldsEngine::setupSDL()
    {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_LINUX_JOYSTICK_DEADZONES, "1");
        SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER);
    }

    Window* WorldsEngine::createWindow()
    {
        windowWidth = 1600;
        windowHeight = 900;
        return new Window("Loading...", 1600, 900, true);
    }


    void WorldsEngine::setupPhysfs(char* argv0)
    {
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

        if (EngineArguments::hasArgument("dataPath"))
        {
            dataStr = EngineArguments::argumentValue("dataPath");
        }

        PHYSFS_init(argv0);
        logVrb("Mounting %s", dataStr.c_str());
        PHYSFS_mount(dataStr.c_str(), "/", 0);
        logVrb("Mounting %s", engineDataStr.c_str());
        PHYSFS_mount(engineDataStr.c_str(), "/", 0);
        logVrb("Mounting %s", srcDataStr.c_str());
        PHYSFS_mount(srcDataStr.c_str(), "/SrcData", 0);
        PHYSFS_setWriteDir(dataStr.c_str());

        PHYSFS_permitSymbolicLinks(1);
    }

    bool inFrame = false;

    int WorldsEngine::eventFilter(void* enginePtr, SDL_Event* evt)
    {
        WorldsEngine* _this = (WorldsEngine*)enginePtr;
        if (evt->type == SDL_WINDOWEVENT &&
            SDL_GetWindowFromID(evt->window.windowID) == _this->window->getWrappedHandle() &&
            (evt->window.event == SDL_WINDOWEVENT_RESIZED || evt->window.event == SDL_WINDOWEVENT_MOVED))
        {
            if (evt->window.event == SDL_WINDOWEVENT_RESIZED)
            {
                Renderer* renderer = _this->renderer.Get();
                _this->windowWidth = evt->window.data1;
                _this->windowHeight = evt->window.data2;
            }

            if (!inFrame)
                _this->runSingleFrame(false);
        }
        return 1;
    }

    void handleDestroyedChild(entt::registry& r, entt::entity ent)
    {
        auto& cc = r.get<ChildComponent>(ent);
        // This will get called both when the child entity is destroyed
        // and when the component is just removed, so return
        // if the parent has already been set to null by
        // removeEntityParent.
        if (!r.valid(cc.parent))
            return;

        // Make sure to not delete the ChildComponent, becuase entt
        // removes it for us
        HierarchyUtil::removeEntityParent(r, ent, false);
    }

    WorldsEngine::WorldsEngine(EngineInitOptions initOptions, char* argv0)
        : pauseSim{false}, running{true}, interfaces{this}
    {
        ZoneScoped;
        PerfTimer startupTimer;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        runAsEditor = initOptions.runAsEditor;
        enableVR = initOptions.enableVR;
        headless = initOptions.dedicatedServer;

#ifndef BUILD_EDITOR
        runAsEditor = false;
#endif

        enki::TaskSchedulerConfig tsc{};
        tsc.numTaskThreadsToCreate =
            workerThreadOverride == -1 ? enki::GetNumHardwareThreads() - 1 : workerThreadOverride;

        g_taskSched.Initialize(tsc);

        if (evtHandler == nullptr)
        {
            // put in a blank event handler instead
            // IGameEventHandler has implemntations for this reason
            evtHandler = new IGameEventHandler;
        }

        // Initialisation Stuffs
        // =====================
        ISplashScreen* splashWindow = nullptr;

        if (!headless)
        {
#ifdef __linux__
            splashWindow = new SplashScreenImplX11(!runAsEditor);
#else
            splashWindow = new SplashScreenImplWin32(!runAsEditor);
#endif
        }

        console = new Console(
            EngineArguments::hasArgument("create-console-window") || headless,
            EngineArguments::hasArgument("enable-console-window-input") || headless
        );

        setupSDL();

        setupPhysfs(argv0);
        if (splashWindow)
        {
            splashWindow->changeOverlay("starting up");
        }

        registry.set<SceneInfo>("Untitled", ~0u);
        registry.on_destroy<ChildComponent>().connect<&handleDestroyedChild>();

        if (splashWindow)
            splashWindow->changeOverlay("loading assetdb");

        AssetDB::load();
        MeshManager::initialize();
        registry.set<SceneSettings>(AssetDB::pathToId("Cubemaps/Miramar/miramar.json"), 1.0f);

        if (!headless)
        {
            // SDL by default will tell desktop environments to disable the compositor
            // when a window is created. If we're just running the editor rather than the game
            // we don't want that
            if (runAsEditor)
                SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

            window = createWindow();

            if (window == nullptr)
            {
                fatalErr("Failed to create window");
                return;
            }

            if (runAsEditor)
                setWindowIcon(window->getWrappedHandle(), "icon_engine.png");
            else
                setWindowIcon(window->getWrappedHandle());

            SDL_SetEventFilter(eventFilter, this);

            window->setTitle(initOptions.gameName);

            if (splashWindow)
                splashWindow->changeOverlay("initialising ui");
        }

        initializeImGui(window, runAsEditor, headless);

        std::vector<std::string> additionalInstanceExts;
        std::vector<std::string> additionalDeviceExts;

        if (enableVR)
        {
            vrInterface = new OpenXRInterface(interfaces);
            interfaces.vrInterface = vrInterface.Get();
            std::vector<std::string> instanceExts = vrInterface->getRequiredInstanceExtensions();
            logMsg("OpenXR asked for the following instance extensions:");
            for (const std::string& s : instanceExts)
            {
                logMsg(" - %s", s.c_str());
            }

            additionalInstanceExts.insert(additionalInstanceExts.begin(), instanceExts.begin(), instanceExts.end());

            std::vector<std::string> deviceExts = vrInterface->getRequiredDeviceExtensions();
            logMsg("OpenXR asked for the following device extensions:");
            for (const std::string& s : deviceExts)
            {
                logMsg(" - %s", s.c_str());
            }

            additionalDeviceExts.insert(additionalDeviceExts.begin(), deviceExts.begin(), deviceExts.end());
        }

        if (splashWindow)
            splashWindow->changeOverlay("initialising renderer");

        if (!headless)
        {
            RendererInitInfo initInfo
            {
                window->getWrappedHandle(),
                additionalInstanceExts,
                additionalDeviceExts,
                enableVR,
                initOptions.gameName,
                interfaces
            };

            bool renderInitSuccess = false;
            renderer = new VKRenderer(initInfo, &renderInitSuccess);
            interfaces.renderer = renderer.Get();

            if (!renderInitSuccess)
            {
                fatalErr("Failed to initialise renderer");
                return;
            }

            if (enableVR)
            {
                vrInterface->init();
                if (!runAsEditor)
                {
                    uint32_t newW, newH;

                    vrInterface->getRenderResolution(&newW, &newH);
                    SDL_Rect rect;
                    SDL_GetDisplayUsableBounds(0, &rect);

                    float scaleFac = glm::min(((float)rect.w * 0.9f) / newW, ((float)rect.h * 0.9f) / newH);

                    window->resize(newW * scaleFac, newH * scaleFac);
                    vrInterface->loadActionJson("VRInput/actions.json");
                }
            }
        }

        cam.position = glm::vec3(0.0f, 0.0f, -1.0f);
        interfaces.mainCamera = &cam;

        inputManager = new InputManager(window->getWrappedHandle());
        window->bindInputManager(inputManager.Get());
        interfaces.inputManager = inputManager.Get();

        scriptEngine = new DotNetScriptEngine(registry, interfaces);
        interfaces.scriptEngine = scriptEngine.Get();

        physicsSystem = new PhysicsSystem(interfaces, registry);
        interfaces.physics = physicsSystem.Get();

        simLoop = new SimulationLoop(interfaces, evtHandler, registry);

        ComponentMetadataManager::setupLookup(&interfaces);

        if (runAsEditor)
        {
            if (splashWindow)
                splashWindow->changeOverlay("initialising editor");
#ifdef BUILD_EDITOR
            editor = new Editor(registry, interfaces);
            interfaces.editor = editor.Get();
#endif
        }

#ifdef BUILD_EDITOR
        if (!scriptEngine->initialise(editor.Get()))
            fatalErr("Failed to initialise .net");
#else
        if (!scriptEngine->initialise(nullptr))
            fatalErr("Failed to initialise .net");
#endif

        inputManager->setScriptEngine(scriptEngine.Get());

        if (!runAsEditor)
            pauseSim = false;

        console->registerCommand(
            [&](const char* arg)
            {
                if (!PHYSFS_exists(arg))
                {
                    logErr(
                        WELogCategoryEngine,
                        "Couldn't find scene %s. Make sure you included the .escn file extension.",
                        arg
                    );
                    return;
                }
                loadScene(AssetDB::pathToId(arg));
            },
            "scene",
            "Loads a scene."
        );

        console->registerCommand(
            [&](const char* arg)
            {
                uint32_t id = (uint32_t)std::atoll(arg);
                if (AssetDB::exists(id))
                    logVrb("Asset %u: %s", id, AssetDB::idToPath(id).c_str());
                else
                    logErr("Nonexistent asset");
            },
            "adb_lookupID",
            "Looks up an asset ID."
        );

        console->registerCommand(
            [&](const char* arg) { timeScale = atof(arg); }, "setTimeScale", "Sets the current time scale."
        );

        if (!headless)
        {
            console->registerCommand(
                [&](const char*)
                {
                    MeshManager::reloadMeshes();
                    physicsSystem->resetMeshCache();
                },
                "reloadContent",
                "Reloads all content."
            );

            console->registerCommand(
                [&](const char*) { MaterialManager::reload(); }, "reloadMaterials", "Reloads materials."
            );

            console->registerCommand(
                [&](const char*)
                {
                    MeshManager::reloadMeshes();
                    physicsSystem->resetMeshCache();
                },
                "reloadMeshes",
                "Reloads meshes."
            );
            console->registerCommand(
                [&](const char*) { renderer->setVsync(!renderer->getVsync()); }, "r_toggleVsync", "Toggles vsync."
            );
        }

        console->registerCommand([&](const char*) { running = false; }, "exit", "Shuts down the engine.");

        console->registerCommand(
            [this](const char* arg)
            {
                std::string argS = arg;
                size_t xPos = argS.find("x");

                if (xPos == std::string::npos)
                {
                    logErr("Invalid window size (specify like 1280x720)");
                    return;
                }
                int width = std::stoi(argS.substr(0, xPos));
                int height = std::stoi(argS.substr(xPos + 1));
                window->resize(width, height);
            },
            "setWindowSize",
            "Sets size of the window."
        );

        if (!headless)
        {
            if (runAsEditor)
                splashWindow->changeOverlay("initialising audio");

            audioSystem = new AudioSystem();
            audioSystem->initialise(registry);

            if (!runAsEditor)
                audioSystem->loadMasterBanks();
        }

        if (!runAsEditor && PHYSFS_exists("CommandScripts/startup.txt"))
            console->executeCommandStr("exec CommandScripts/startup");

        if (evtHandler != nullptr)
        {
            evtHandler->init(registry, interfaces);

            if (!runAsEditor)
            {
                evtHandler->onSceneStart(registry);

                scriptEngine->onSceneStart();
            }
        }

        if (!headless)
        {
            delete splashWindow;

            window->show();
            window->raise();
        }

        if (headless)
        {
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

        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        logMsg("Engine startup took about %.3fms", startupTimer.stopGetMs());
    }

    void WorldsEngine::run()
    {
        interFrameInfo.frameCounter = 0;
        interFrameInfo.lastPerfCounter = SDL_GetPerformanceCounter();
        interFrameInfo.lastUpdateTime = 0.0;

        while (running)
        {
            runSingleFrame(true);
        }
    }

    void drawFPSCounter(float deltaTime)
    {
        auto drawList = ImGui::GetForegroundDrawList();

        char buf[128];
        snprintf(buf, 128, "%.1f fps\n%.3fms", 1.0f / deltaTime, deltaTime * 1000.0f);

        auto bgSize = ImGui::CalcTextSize(buf);
        auto pos = ImGui::GetMainViewport()->Pos + ImVec2(ImGui::GetMainViewport()->Size.x, 0.0f);
        auto topLeftCorner = pos - ImVec2(bgSize.x, 0.0f);

        drawList->AddRectFilled(topLeftCorner, pos + ImVec2(0.0f, bgSize.y), ImColor(0.0f, 0.0f, 0.0f, 0.5f));

        ImColor col{1.0f, 1.0f, 1.0f};

        if (deltaTime > 0.011111f)
            col = ImColor{1.0f, 0.0f, 0.0f};
        else if (deltaTime > 0.017f)
            col = ImColor{0.75f, 0.75f, 0.0f};

        drawList->AddText(topLeftCorner, ImColor(1.0f, 1.0f, 1.0f), buf);
    }

    void WorldsEngine::runSingleFrame(bool processEvents)
    {
        ZoneScoped;
        uint64_t now = SDL_GetPerformanceCounter();

        uint64_t deltaTicks = now - interFrameInfo.lastPerfCounter;
        interFrameInfo.lastPerfCounter = now;
        interFrameInfo.deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
        gameTime += interFrameInfo.deltaTime;

        uint64_t updateStart = SDL_GetPerformanceCounter();

        if (processEvents)
            window->processEvents();
        if (window->shouldQuit())
            running = false;

        if (vrInterface)
        {
            vrInterface->waitFrame(); 
            vrInterface->beginFrame(); 
        }

        if (!headless)
        {
            ImGui_ImplSDL2_NewFrame(window->getWrappedHandle());
        }

        inFrame = true;

        ImGui::NewFrame();

        inputManager->update();

        if (vrInterface)
            vrInterface->updateEvents();

        float interpAlpha = 1.0f;

        if (!runAsEditor EDITORONLY(|| editor->isPlaying()))
        {
            evtHandler->preSimUpdate(registry, interFrameInfo.deltaTime);
        }

        double simTime = 0.0;
        bool didSimRun = false;
        if (!pauseSim)
        {
            PerfTimer perfTimer;
            bool physicsOnly = false EDITORONLY(|| (editor && !editor->isPlaying()));
            didSimRun = simLoop->updateSimulation(interpAlpha, timeScale, interFrameInfo.deltaTime,
                                                  physicsOnly);
            simTime = perfTimer.stopGetMs();
        }

        if (!runAsEditor EDITORONLY(|| editor->isPlaying()))
        {
            evtHandler->update(registry, interFrameInfo.deltaTime * timeScale, interpAlpha);
            scriptEngine->onUpdate(interFrameInfo.deltaTime * timeScale, interpAlpha);
        }

        if (!headless)
        {
            windowSize.x = windowWidth;
            windowSize.y = windowHeight;

            if (runAsEditor)
            {
                EDITORONLY(editor->update((float)interFrameInfo.deltaTime));
            }
        }

        if (inputManager->keyPressed(SDL_SCANCODE_RCTRL, true))
        {
            inputManager->lockMouse(!inputManager->mouseLockState());
        }

        if (inputManager->keyPressed(SDL_SCANCODE_F3, true))
        {
            renderer->reloadShaders();
        }

        if (inputManager->keyPressed(SDL_SCANCODE_F11, true))
        {
            window->setFullscreen(!window->isFullscreen());
        }

        uint64_t updateEnd = SDL_GetPerformanceCounter();

        uint64_t updateLength = updateEnd - updateStart;
        double updateTime = updateLength / (double)SDL_GetPerformanceFrequency();

        DebugTimeInfo dti{
            .deltaTime = interFrameInfo.deltaTime,
            .updateTime = updateTime,
            .simTime = simTime,
            .didSimRun = didSimRun,
            .lastUpdateTime = interFrameInfo.lastUpdateTime,
            .lastTickRendererTime = interFrameInfo.lastTickRendererTime,
            .frameCounter = interFrameInfo.frameCounter,
        };

        drawDebugInfoWindow(interfaces, dti);

        static ConVar drawFPS{"drawFPS", "0", "Draws a simple FPS counter in the corner of the screen."};
        if (drawFPS.getInt())
        {
            drawFPSCounter(dti.deltaTime);
        }

        if (glm::any(glm::isnan(cam.position)))
        {
            cam.position = glm::vec3{0.0f};
            logWarn("cam.position was NaN!");
        }

        if (!headless)
        {
            glm::vec3 alPos = cam.position;
            glm::quat alRot = cam.rotation;

            auto view = registry.view<AudioListenerOverride>();
            if (view.size() > 0)
            {
                auto overrideEnt = view.front();
                auto overrideT = registry.get<Transform>(overrideEnt);
                alPos = overrideT.position;
                alRot = overrideT.rotation;
            }
            audioSystem->update(registry, alPos, alRot, interFrameInfo.deltaTime);
        }

        console->drawWindow();

        registry.view<ChildComponent, Transform>().each(
            [&](ChildComponent& c, Transform& t)
            {
                if (!registry.valid(c.parent))
                    return;
                t = c.offset.transformBy(registry.get<Transform>(c.parent));
                t.scale = c.offset.scale * registry.get<Transform>(c.parent).scale;
            }
        );

        if (!headless)
        {
            PerfTimer rpt{};
            tickRenderer(interFrameInfo.deltaTime, true);
            interFrameInfo.lastTickRendererTime = rpt.stopGetMs();
            if (vrInterface)
                vrInterface->endFrame();
        }

        interFrameInfo.frameCounter++;

        inputManager->endFrame();

        for (auto& e : nextFrameKillList)
        {
            if (registry.valid(e))
                registry.destroy(e);
        }

        nextFrameKillList.clear();

        if (sceneLoadQueued)
        {
            sceneLoadQueued = false;
            SceneInfo& si = registry.ctx<SceneInfo>();
            si.name = std::filesystem::path(AssetDB::idToPath(queuedSceneID)).stem().string();
            si.id = queuedSceneID;
            PHYSFS_File* file = AssetDB::openAssetFileRead(queuedSceneID);
            SceneLoader::loadScene(file, registry);

            // TODO: Load content here

            if (!runAsEditor EDITORONLY(|| editor->isPlaying()))
            {
                evtHandler->onSceneStart(registry);

                scriptEngine->onSceneStart();
            }

            registry.view<OldAudioSource>().each(
                [](OldAudioSource& as)
                {
                    if (as.playOnSceneOpen)
                    {
                        as.isPlaying = true;
                    }
                }
            );

            registry.view<AudioSource>().each(
                [](AudioSource& as)
                {
                    if (as.playOnSceneStart)
                        as.eventInstance->start();
                }
            );
        }

        uint64_t postUpdate = SDL_GetPerformanceCounter();
        double completeUpdateTime = (postUpdate - now) / (double)SDL_GetPerformanceFrequency();

        if (headless)
        {
            double waitTime = g_console->getConVar("sim_stepTime")->getFloat() - completeUpdateTime;
            if (waitTime > 0.0)
                SDL_Delay(waitTime * 1000);
        }

        interFrameInfo.lastUpdateTime = updateTime;
        inFrame = false;
    }

    void WorldsEngine::tickRenderer(float deltaTime, bool renderImGui)
    {
        ZoneScoped;
        const physx::PxRenderBuffer& pxRenderBuffer = physicsSystem->scene()->getRenderBuffer();

        for (uint32_t i = 0; i < pxRenderBuffer.getNbLines(); i++)
        {
            const physx::PxDebugLine& line = pxRenderBuffer.getLines()[i];
            drawLine(px2glm(line.pos0), px2glm(line.pos1), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        }

        if (renderImGui)
        {
            ZoneScopedN("ImGui render update");

            ImGui::Render();

            if (window->isMaximised())
            {
                ImGui::GetDrawData()->RenderOffset = ImVec2(8, 8);
            }

            renderer->setImGuiDrawData(ImGui::GetDrawData());

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        renderer->frame(registry, deltaTime);

        // who is mark and why are we framing him?
        FrameMark;
    }

    void WorldsEngine::loadScene(AssetID scene)
    {
        if (!AssetDB::exists(scene))
        {
            logErr("Tried to load scene that doesn't exist!");
            return;
        }

        sceneLoadQueued = true;
        queuedSceneID = scene;
    }

    WorldsEngine::~WorldsEngine()
    {
        audioSystem->shutdown(registry);
        if (evtHandler != nullptr && !runAsEditor)
            evtHandler->shutdown(registry);

        registry.clear();

#ifdef BUILD_EDITOR
        if (runAsEditor)
            editor.Reset();
#endif

        if (vrInterface)
            vrInterface.Reset();

        if (renderer)
            renderer.Reset();

        PHYSFS_deinit();
        logVrb("Quitting SDL.");
        SDL_Quit();
    }
}
