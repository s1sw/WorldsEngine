#include <SDL.h>
#include <atomic>
#include <iostream>
#include <thread>
#include "JobSystem.hpp"
#include <Core/Window.hpp>
#include <Core/ISystem.hpp>
#include "Engine.hpp"
#include <ImGui/imgui.h>
#include <physfs.h>
#include <ImGui/imgui_impl_sdl.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <entt/entt.hpp>
#include <Scripting/NetVM.hpp>
#include "Transform.hpp"
#include <Physics/Physics.hpp>
#include <Input/Input.hpp>
#include <Physics/PhysicsActor.hpp>
#include <glm/gtx/norm.hpp>
#include <physx/PxQueryReport.h>
#include "tracy/Tracy.hpp"
#include <Editor/Editor.hpp>
#include <VR/OpenVRInterface.hpp>
#include <Core/Log.hpp>
#include <Audio/Audio.hpp>
#include <stb_image.h>
#include <Core/Console.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Render/Render.hpp>
#include <Util/TimingUtil.hpp>
#include <Util/VKImGUIUtil.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui_internal.h>
#include <Util/CreateModelObject.hpp>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <Util/RichPresence.hpp>
#include "EarlySDLUtil.hpp"
#include <Render/ShaderCache.hpp>
#include "readerwriterqueue.h"
#include <ComponentMeta/ComponentMetadata.hpp>
#include <Util/EnumUtil.hpp>
#include "SplashScreenImpls/ISplashScreen.hpp"
#ifdef __linux__
#include "SplashScreenImpls/SplashScreenX11.hpp"
#else
#include "SplashScreenImpls/SplashScreenWin32.hpp"
#endif
#include <ImGui/imgui_freetype.h>
#include <UI/WorldTextComponent.hpp>
#include <Editor/GuiUtil.hpp>
#include <Render/DebugLines.hpp>
#undef min
#undef max

#ifdef CHECK_NEW_DELETE
robin_hood::unordered_map<void*, size_t> allocatedPtrs;
size_t allocatedMem = 0;
size_t liveAllocations = 0;

void* operator new(size_t count) {
    void* ptr = malloc(count);
#ifdef TRACY_ENABLE
    TracyAlloc(ptr, count);
#endif
    allocatedPtrs.insert({ptr, count});
    allocatedMem += count;
    liveAllocations++;
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr == nullptr) return;
    if (!allocatedPtrs.contains(ptr)) {
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

namespace worlds {
    uint32_t fullscreenToggleEventId;
    uint32_t showWindowEventId;

    int workerThreadOverride = -1;
    bool enableOpenVR = false;
    glm::ivec2 windowSize;

    // event thread sync
    moodycamel::ReaderWriterQueue<SDL_Event> evts;

    void WorldsEngine::setupSDL() {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_LINUX_JOYSTICK_DEADZONES, "1");
        SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER);
    }

    Window* WorldsEngine::createWindow() {
        windowWidth = 1600;
        windowHeight = 900;
        return new Window("Loading...", 1600, 900, true);
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

        _this->window = _this->createWindow();
        if (_this->window == nullptr) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "err", SDL_GetError(), NULL);
        }

        if (_this->runAsEditor)
            setWindowIcon(_this->window->getWrappedHandle(), "icon_engine.png");
        else
            setWindowIcon(_this->window->getWrappedHandle());

        SDL_SetEventFilter(eventFilter, _this);

        windowCreated = true;
        while (_this->running) {
            SDL_PumpEvents();
            SDL_Delay(1);
        }

        logVrb(WELogCategoryEngine, "window thread exiting");
        // SDL requires threads to return an int
        return 0;
    }

    ImFont* addImGuiFont(std::string fontPath, float size, ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {
        PHYSFS_File* ttfFile = PHYSFS_openRead(fontPath.c_str());
        if (ttfFile == nullptr) {
            logWarn("Couldn't open font file");
            return nullptr;
        }

        auto fileLength = PHYSFS_fileLength(ttfFile);

        if (fileLength == -1) {
            PHYSFS_close(ttfFile);
            logWarn("Couldn't determine size of editor font file");
            return nullptr;
        }

        void* buf = std::malloc(fileLength);
        auto readBytes = PHYSFS_readBytes(ttfFile, buf, fileLength);

        if (readBytes != fileLength) {
            PHYSFS_close(ttfFile);
            logWarn("Failed to read full TTF file");
            return nullptr;
        }

        PHYSFS_close(ttfFile);

        ImFontConfig defaultConfig{};

        if (config) {
            memcpy(config->Name, fontPath.c_str(), fontPath.size());
        } else {
            for (size_t i = 0; i < fontPath.size(); i++) {
                defaultConfig.Name[i] = fontPath[i];
            }
            config = &defaultConfig;
        }

        return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, (int)readBytes, size, config, ranges);
    }

    JobSystem* g_jobSys;

    robin_hood::unordered_map<entt::entity, physx::PxTransform> currentState;
    robin_hood::unordered_map<entt::entity, physx::PxTransform> previousState;

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
        logVrb("Mounting %s", dataStr.c_str());
        PHYSFS_mount(dataStr.c_str(), "/", 0);
        logVrb("Mounting %s", engineDataStr.c_str());
        PHYSFS_mount(engineDataStr.c_str(), "/", 0);
        logVrb("Mounting %s", srcDataStr.c_str());
        PHYSFS_mount(srcDataStr.c_str(), "/SrcData", 0);
        PHYSFS_setWriteDir(dataStr.c_str());

        PHYSFS_permitSymbolicLinks(1);
    }

    extern void loadDefaultUITheme();
    extern ImFont* boldFont;

    void setupUIFonts(float dpiScale) {
        if (PHYSFS_exists("Fonts/EditorFont.ttf"))
            ImGui::GetIO().Fonts->Clear();

        boldFont = addImGuiFont("Fonts/EditorFont-Bold.ttf", 20.0f * dpiScale);
        ImGui::GetIO().FontDefault = addImGuiFont("Fonts/EditorFont.ttf", 20.0f * dpiScale);

        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.OversampleH = 1;

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAR, 17.0f * dpiScale, &iconConfig, iconRanges);
        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAS, 17.0f * dpiScale, &iconConfig, iconRanges);

        ImFontConfig iconConfig2{};
        iconConfig2.MergeMode = true;
        iconConfig2.PixelSnapH = true;
        iconConfig2.OversampleH = 2;
        iconConfig2.GlyphOffset = ImVec2(-3.0f, 5.0f);
        iconConfig2.GlyphExtraSpacing = ImVec2(-5.0f, 0.0f);

        static const ImWchar iconRangesFAD[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAD, 22.0f * dpiScale, &iconConfig2, iconRangesFAD);

#ifdef _WIN32
        ImFontConfig emojiFontConfig{};
        emojiFontConfig.MergeMode = true;
        emojiFontConfig.OversampleH = emojiFontConfig.OversampleV = 1;
        emojiFontConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
        static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };

        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 17.0f * dpiScale, &emojiFontConfig, ranges);
#endif
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

    entt::registry renderRegistry;
    ImDrawData renderThreadDrawData;
    Camera renderThreadCamera;
    std::mutex rendererLock;
    std::condition_variable renderThreadCV;
    bool renderThreadWorkReady = false;
    bool renderThreadAvailable = true;
    ConVar pacingSleepTime{ "pacingSleepTime", "0" };


    int WorldsEngine::renderThread(void* data) {
        WorldsEngine* _this = (WorldsEngine*)data;

        while (_this->running) {
            ZoneScoped;
            std::unique_lock<std::mutex> lg {rendererLock};
            renderThreadCV.wait(lg, [] {return renderThreadWorkReady;});
            renderThreadWorkReady = false;
            renderThreadAvailable = false;

            if (!_this->running) break;

            _this->renderer->setImGuiDrawData(&renderThreadDrawData);
            _this->renderer->frame(renderThreadCamera, renderRegistry);

            for (int i = 0; i < renderThreadDrawData.CmdListsCount; i++) {
                IM_DELETE(renderThreadDrawData.CmdLists[i]);
            }

            renderThreadAvailable = true;
            lg.unlock();
            renderThreadCV.notify_one();
        }

        return 0;
    }

    bool inFrame = false;

    int WorldsEngine::eventFilter(void* enginePtr, SDL_Event* evt) {
        WorldsEngine* _this = (WorldsEngine*)enginePtr;
        if (evt->type == SDL_WINDOWEVENT && SDL_GetWindowFromID(evt->window.windowID) == _this->window->getWrappedHandle() && (evt->window.event == SDL_WINDOWEVENT_RESIZED || evt->window.event == SDL_WINDOWEVENT_MOVED)) {
            if (evt->window.event == SDL_WINDOWEVENT_RESIZED) {
                Renderer* renderer = _this->renderer.get();
                _this->windowWidth = evt->window.data1;
                _this->windowHeight = evt->window.data2;

                renderer->recreateSwapchain(evt->window.data1, evt->window.data2);
            }

            {
                std::unique_lock<std::mutex> lg(rendererLock);
                renderThreadCV.wait(lg, []{return renderThreadAvailable;});
            }

            if (!inFrame)
                _this->runSingleFrame(false);
        }
        return 1;
    }

    template <typename T>
    void cloneComponent(entt::registry& src, entt::registry& dst) {
        auto view = src.view<T>();

        if constexpr (std::is_empty<T>::value) {
            dst.insert<T>(view.data(), view.data() + view.size());
        } else {
            dst.insert<T>(view.data(), view.data() + view.size(), view.raw(), view.raw() + view.size());
        }
    }

    SDL_Thread* renderThreadInstance = nullptr;
    bool screenPassIsVR = false;
    float screenPassResScale = 1.0f;

    WorldsEngine::WorldsEngine(EngineInitOptions initOptions, char* argv0)
        : pauseSim{ false }
        , running{ true }
        , simAccumulator{ 0.0 } {
        ZoneScoped;
        PerfTimer startupTimer;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        runAsEditor = initOptions.runAsEditor;
        enableOpenVR = initOptions.enableVR;
        dedicatedServer = initOptions.dedicatedServer;

        // Initialisation Stuffs
        // =====================
        ISplashScreen* splashWindow = nullptr;

        if (!dedicatedServer) {
#ifdef __linux__
            splashWindow = new SplashScreenImplX11(!runAsEditor);
#else
            splashWindow = new SplashScreenImplWin32(!runAsEditor);
#endif
        }

        console = std::make_unique<Console>(
            EngineArguments::hasArgument("create-console-window") || dedicatedServer,
            EngineArguments::hasArgument("enable-console-window-input") || dedicatedServer);

        setupSDL();

        setupPhysfs(argv0);
        if (!dedicatedServer && runAsEditor) {
            splashWindow->changeOverlay("starting up");
        }

        fullscreenToggleEventId = SDL_RegisterEvents(1);
        showWindowEventId = SDL_RegisterEvents(1);

        // Ensure that we have a minimum of two workers, as one worker
        // means that jobs can be missed
        g_jobSys = new JobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };

        registry.set<SceneInfo>("Untitled", ~0u);

        if (!dedicatedServer && runAsEditor)
            splashWindow->changeOverlay("loading assetdb");

        AssetDB::load();
        registry.set<SceneSettings>(AssetDB::pathToId("Cubemaps/Miramar/miramar.json"), 1.0f);

        if (!dedicatedServer) {
            //SDL_DetachThread(SDL_CreateThread(windowThread, "Window Thread", this));
            //while(!windowCreated){}
            if (runAsEditor)
                SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
            window = createWindow();
            if (window == nullptr) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "err", SDL_GetError(), NULL);
            }

            if (runAsEditor)
                setWindowIcon(window->getWrappedHandle(), "icon_engine.png");
            else
                setWindowIcon(window->getWrappedHandle());

            SDL_SetEventFilter(eventFilter, this);

            windowCreated = true;

            window->setTitle(initOptions.gameName);

            if (runAsEditor)
                splashWindow->changeOverlay("initialising ui");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
        io.IniFilename = runAsEditor ? "imgui_editor.ini" : "imgui.ini";
        io.Fonts->TexDesiredWidth = 512;

        if (!dedicatedServer) {
            ImGui_ImplSDL2_InitForVulkan(window->getWrappedHandle());

            float dpiScale = 1.0f;
            int dI = SDL_GetWindowDisplayIndex(window->getWrappedHandle());
            float ddpi, hdpi, vdpi;
            if (SDL_GetDisplayDPI(dI, &ddpi, &hdpi, &vdpi) != -1) {
                dpiScale = ddpi / 96.0f;
            }

            setupUIFonts(dpiScale);

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

                window->resize(newW * scaleFac, newH * scaleFac);
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
                window->getWrappedHandle(),
                additionalInstanceExts, additionalDeviceExts,
                enableOpenVR, activeApi, vrInterface,
                initOptions.gameName
            };

            bool renderInitSuccess = false;
            renderer = std::make_unique<VKRenderer>(initInfo, &renderInitSuccess);

            renderThreadInstance = SDL_CreateThread(renderThread, "Render Thread", this);

            if (!renderInitSuccess) {
                fatalErr("Failed to initialise renderer");
                return;
            }
        }

        cam.position = glm::vec3(0.0f, 0.0f, -1.0f);

        inputManager = std::make_unique<InputManager>(window->getWrappedHandle());
        window->bindInputManager(inputManager.get());

        interfaces = EngineInterfaces {
            .vrInterface = enableOpenVR ? openvrInterface.get() : nullptr,
            .renderer = renderer.get(),
            .mainCamera = &cam,
            .inputManager = inputManager.get(),
            .engine = this
        };

        if (!dedicatedServer) {
            VKImGUIUtil::createObjects(static_cast<VKRenderer*>(renderer.get())->getHandles());
        }

        scriptEngine = std::make_unique<DotNetScriptEngine>(registry, interfaces);
        interfaces.scriptEngine = scriptEngine.get();
        
        physicsSystem = std::make_unique<PhysicsSystem>(interfaces, registry);
        interfaces.physics = physicsSystem.get();

        if (!dedicatedServer && runAsEditor) {
            splashWindow->changeOverlay("initialising editor");
            editor = std::make_unique<Editor>(registry, interfaces);
        } else {
            ComponentMetadataManager::setupLookup(&interfaces);
        }

        interfaces.editor = editor.get();

        if (!scriptEngine->initialise(editor.get()))
            fatalErr("Failed to initialise .net");

        inputManager->setScriptEngine(scriptEngine.get());

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
                logVrb("Asset %u: %s", id, AssetDB::idToPath(id).c_str());
            else
                logErr("Nonexistent asset");
        }, "adb_lookupID", "Looks up an asset ID.", nullptr);

        console->registerCommand([&](void*, const char* arg) {
            timeScale = atof(arg);
        }, "setTimeScale", "Sets the current time scale.", nullptr);

        if (!dedicatedServer) {
            console->registerCommand([&](void*, const char*) {
                screenPassIsVR = (!screenRTTPass) && enableOpenVR;
                }, "toggleVRRendering", "Toggle whether the screen RTT pass has VR enabled.");

            if (runAsEditor) {

                console->registerCommand([&](void*, const char* arg) {
                    screenPassResScale = std::atof(arg);
                }, "setGameResScale", "Sets the resolution scale of the game view.");
            }

            console->registerCommand([&](void*, const char*) {
                MeshManager::reloadMeshes();
                renderer->reloadContent(ReloadFlags::All);
                physicsSystem->resetMeshCache();
            }, "reloadContent", "Reloads all content.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Textures);
            }, "reloadTextures", "Reloads textures.");

            console->registerCommand([&](void*, const char*) {
                renderer->reloadContent(ReloadFlags::Materials);
            }, "reloadMaterials", "Reloads materials.");

            console->registerCommand([&](void*, const char*) {
                MeshManager::reloadMeshes();
                renderer->reloadContent(ReloadFlags::Meshes);
                physicsSystem->resetMeshCache();
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
            window->resize(width, height);
        }, "setWindowSize", "Sets size of the window.", nullptr);

        console->registerCommand([this](void*, const char* arg) {
            MessagePackSceneSerializer::saveScene("msgpack_test.wscn", registry);
        }, "saveMsgPack", "Saves the current scene in MessagePack format.");

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

            if (!runAsEditor)
                audioSystem->loadMasterBanks();
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
                screenPassIsVR = true;
            }

            RTTPassCreateInfo screenRTTCI {
                .width = w,
                .height = h,
                .resScale = 1.0f,
                .isVr = enableOpenVR,
                .useForPicking = false,
                .enableShadows = true,
                .outputToScreen = true,
            };

            screenRTTPass = renderer->createRTTPass(screenRTTCI);
            logVrb("Created screen RTT pass");

            delete splashWindow;

            window->show();
            window->raise();
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

        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        logMsg("Engine startup took about %.3fms", startupTimer.stopGetMs());
    }

    void WorldsEngine::createStartupScene() {
        registry.clear();
        AssetID grassMatId = AssetDB::pathToId("Materials/DevTextures/dev_green.json");
        AssetID devMatId = AssetDB::pathToId("Materials/DevTextures/dev_blue.json");

        AssetID modelId = AssetDB::pathToId("Models/cube.wmdl");
        AssetID monkeyId = AssetDB::pathToId("Models/monkey.wmdl");
        renderer->preloadMesh(modelId);
        renderer->preloadMesh(monkeyId);
        createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, grassMatId, glm::vec3(5.0f, 1.0f, 5.0f));

        createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, devMatId);

        entt::entity dirLightEnt = registry.create();
        registry.emplace<WorldLight>(dirLightEnt, LightType::Directional);
        registry.emplace<Transform>(dirLightEnt, glm::vec3(0.0f), glm::angleAxis(glm::radians(90.01f), glm::vec3(1.0f, 0.0f, 0.0f)));
        registry.set<SceneInfo>("Untitled", INVALID_ASSET);
    }

    void WorldsEngine::mainLoop() {
        interFrameInfo.frameCounter = 0;
        interFrameInfo.lastPerfCounter = SDL_GetPerformanceCounter();
        interFrameInfo.lastUpdateTime = 0.0;

        renderer->recreateSwapchain();
        while (running) {
            runSingleFrame(true);
        }
    }

    void WorldsEngine::runSingleFrame(bool processEvents) {
        uint64_t now = SDL_GetPerformanceCounter();

        uint64_t deltaTicks = now - interFrameInfo.lastPerfCounter;
        interFrameInfo.lastPerfCounter = now;
        interFrameInfo.deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
        gameTime += interFrameInfo.deltaTime;

        if (renderer->getVsync() || enableOpenVR) {
            double targetTime;

            if (enableOpenVR) {
                float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
                targetTime = 1.0 / fDisplayFrequency;
            } else {
                int displayIdx = SDL_GetWindowDisplayIndex(window->getWrappedHandle());
                SDL_DisplayMode displayMode;
                SDL_GetCurrentDisplayMode(displayIdx, &displayMode);
                targetTime = 1.0 / displayMode.refresh_rate;
            }

            // Leave about 3ms of margin just in case
            targetTime -= 0.003;

            double timeAtStart = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
            double throttleTime = pacingSleepTime.getInt() ? pacingSleepTime.getInt() / 1000.0 : glm::clamp(targetTime - interFrameInfo.lastUpdateTime, 0.0, 0.02);
            double currTime = timeAtStart;

            while (currTime - timeAtStart < throttleTime) {
                currTime = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
                if (currTime - timeAtStart > 0.001) {
                    SDL_Delay(1);
                }
            }
        }

        uint64_t updateStart = SDL_GetPerformanceCounter();

        if (processEvents)
            window->processEvents();
        if (window->shouldQuit())
            running = false;

        if (!dedicatedServer) {
            tickRichPresence();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame(window->getWrappedHandle());
        }
        inFrame = true;

        ImVec2 newFrameDisplaySize(windowWidth, windowHeight);
        if (window->isMaximised()) {
            newFrameDisplaySize.x -= 16;
            newFrameDisplaySize.y -= 16;
        }

        //ImGui::GetIO().DisplaySize = newFrameDisplaySize;
        ImGui::GetMainViewport()->Size = newFrameDisplaySize;

        ImGui::NewFrame();
        ImGui::GetMainViewport()->Size = newFrameDisplaySize;

        inputManager->update();

        if (openvrInterface)
            openvrInterface->updateInput();

        if (!dedicatedServer) {
            screenRTTPass->active = !runAsEditor || !editor->active;
        }

        if (enableOpenVR) {
            static bool lastIsVR = true;
            uint32_t w = 1600;
            uint32_t h = 900;

            if (screenPassIsVR) {
                openvrInterface->getRenderResolution(&w, &h);
            }

            if (screenPassIsVR != lastIsVR || w != screenRTTPass->width || h != screenRTTPass->height) {
                renderer->destroyRTTPass(screenRTTPass);

                RTTPassCreateInfo screenRTTCI {
                    .width = w,
                    .height = h,
                    .resScale = screenPassResScale,
                    .isVr = screenPassIsVR,
                    .useForPicking = false,
                    .enableShadows = true,
                    .outputToScreen = true,
                };

                screenRTTPass = renderer->createRTTPass(screenRTTCI);
            }
        }

        screenRTTPass->setResolutionScale(screenPassResScale);

        float interpAlpha = 1.0f;

        if (evtHandler != nullptr && (!runAsEditor || !editor->active)) {
            evtHandler->preSimUpdate(registry, interFrameInfo.deltaTime);

            for (auto* system : systems)
                system->preSimUpdate(registry, interFrameInfo.deltaTime);
        }

        double simTime = 0.0;
        if (!pauseSim) {
            PerfTimer perfTimer;
            updateSimulation(interpAlpha, interFrameInfo.deltaTime);
            simTime = perfTimer.stopGetMs();
        }

        if (evtHandler != nullptr && !(runAsEditor && editor->active)) {
            for (auto* system : systems)
                system->update(registry, interFrameInfo.deltaTime * timeScale, interpAlpha);

            evtHandler->update(registry, interFrameInfo.deltaTime * timeScale, interpAlpha);
            scriptEngine->onUpdate(interFrameInfo.deltaTime * timeScale, interpAlpha);
        }

        if (!dedicatedServer) {
            windowSize.x = windowWidth;
            windowSize.y = windowHeight;

            if (runAsEditor) {
                editor->update((float)interFrameInfo.deltaTime);
            }
        }

        if (inputManager->keyPressed(SDL_SCANCODE_RCTRL, true)) {
            inputManager->lockMouse(!inputManager->mouseLockState());
        }

        if (inputManager->keyPressed(SDL_SCANCODE_F3, true)) {
            ShaderCache::clear();
            renderer->recreateSwapchain();
        }

        if (inputManager->keyPressed(SDL_SCANCODE_F11, true)) {
            //SDL_Event evt;
            //SDL_zero(evt);
            //evt.type = fullscreenToggleEventId;
            //SDL_PushEvent(&evt);
            window->setFullscreen(true);
        }

        uint64_t updateEnd = SDL_GetPerformanceCounter();

        uint64_t updateLength = updateEnd - updateStart;
        double updateTime = updateLength / (double)SDL_GetPerformanceFrequency();

        DebugTimeInfo dti {
            .deltaTime = interFrameInfo.deltaTime,
            .updateTime = updateTime,
            .simTime = simTime,
            .lastUpdateTime = interFrameInfo.lastUpdateTime,
            .frameCounter = interFrameInfo.frameCounter
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

            auto view = registry.view<AudioListenerOverride>();
            if (view.size() > 0) {
                auto overrideEnt = view.front();
                auto overrideT = registry.get<Transform>(overrideEnt);
                alPos = overrideT.position;
                alRot = overrideT.rotation;
            }
            audioSystem->update(registry, alPos, alRot, interFrameInfo.deltaTime);
        }

        console->drawWindow();

        registry.view<ChildComponent, Transform>().each([&](ChildComponent& c, Transform& t) {
            if (!registry.valid(c.parent)) return;
            t = c.offset.transformBy(registry.get<Transform>(c.parent));
            });

        if (!dedicatedServer) {
            ZoneScopedN("Copy to Render Thread");
            tickRenderer(true);
        }

        g_jobSys->completeFrameJobs();
        interFrameInfo.frameCounter++;

        inputManager->endFrame();

        for (auto& e : nextFrameKillList) {
            if (registry.valid(e))
                registry.destroy(e);
        }

        nextFrameKillList.clear();

        if (sceneLoadQueued) {
            if (enableOpenVR) {
                vr::VRCompositor()->FadeGrid(0.1f, true);
            }
            sceneLoadQueued = false;
            SceneInfo& si = registry.ctx<SceneInfo>();
            si.name = std::filesystem::path(AssetDB::idToPath(queuedSceneID)).stem().string();
            si.id = queuedSceneID;
            PHYSFS_File* file = AssetDB::openAssetFileRead(queuedSceneID);
            SceneLoader::loadScene(file, registry);

            std::unique_lock<std::mutex> lg{rendererLock};
            renderThreadCV.wait(lg, []{ return renderThreadAvailable; });

            if (renderer) {
                renderer->uploadSceneAssets(registry);
                renderer->unloadUnusedAssets(registry);
            }

            if (evtHandler && (!runAsEditor || !editor->active)) {
                evtHandler->onSceneStart(registry);

                for (auto* system : systems)
                    system->onSceneStart(registry);

                scriptEngine->onSceneStart();
            }

            registry.view<OldAudioSource>().each([](OldAudioSource& as) {
                if (as.playOnSceneOpen) {
                    as.isPlaying = true;
                }
            });

            registry.view<AudioSource>().each([](AudioSource& as) {
                if (as.playOnSceneStart)
                    as.eventInstance->start();
            });

            if (enableOpenVR) {
                vr::VRCompositor()->FadeGrid(0.1f, false);
            }
        }

        uint64_t postUpdate = SDL_GetPerformanceCounter();
        double completeUpdateTime = (postUpdate - now) / (double)SDL_GetPerformanceFrequency();

        if (dedicatedServer) {
            double waitTime = simStepTime.getFloat() - completeUpdateTime;
            if (waitTime > 0.0)
                SDL_Delay(waitTime * 1000);
        }

        interFrameInfo.lastUpdateTime = updateTime;
        inFrame = false;
    }

    void WorldsEngine::tickRenderer(bool renderImGui) {
        ZoneScoped;
        std::unique_lock<std::mutex> lg(rendererLock);
        const physx::PxRenderBuffer& pxRenderBuffer = physicsSystem->scene()->getRenderBuffer();

        for (uint32_t i = 0; i < pxRenderBuffer.getNbLines(); i++) {
            const physx::PxDebugLine& line = pxRenderBuffer.getLines()[i];
            drawLine(px2glm(line.pos0), px2glm(line.pos1), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        }

        {
            ZoneScopedN("Waiting on CV");
            renderThreadCV.wait(lg, [this]{return renderThreadAvailable || !running;});
        }

        {
            ZoneScopedN("Copying entities and components");
            renderRegistry.clear();
            renderRegistry.assign(registry.data(), registry.data() + registry.size(), registry.destroyed());
            cloneComponent<Transform>(registry, renderRegistry);
            cloneComponent<WorldObject>(registry, renderRegistry);
            cloneComponent<SkinnedWorldObject>(registry, renderRegistry);
            cloneComponent<WorldLight>(registry, renderRegistry);
            cloneComponent<WorldCubemap>(registry, renderRegistry);
            cloneComponent<UseWireframe>(registry, renderRegistry);
            cloneComponent<EditorGlow>(registry, renderRegistry);
            cloneComponent<ProxyAOComponent>(registry, renderRegistry);
            cloneComponent<SphereAOProxy>(registry, renderRegistry);
            cloneComponent<WorldTextComponent>(registry, renderRegistry);
        }

        renderRegistry.set<SceneSettings>(registry.ctx<SceneSettings>());

        if (renderImGui) {
            ZoneScopedN("Copy ImGui data");
            ImGui::Render();
            renderThreadDrawData = *ImGui::GetDrawData();
            for (int i = 0; i < renderThreadDrawData.CmdListsCount; i++) {
                ImDrawList* original = renderThreadDrawData.CmdLists[i];
                renderThreadDrawData.CmdLists[i] = original->CloneOutput();
            }

            if (window->isMaximised()) {
                renderThreadDrawData.DisplayPos += ImVec2(-9, -9);
            }

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        renderThreadCamera = cam;

        renderThreadWorkReady = true;

        lg.unlock();
        renderThreadCV.notify_one();
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
                        0.0f, FLT_MAX, ImVec2(0.0f, 125.0f));
                    ImGui::Text("Frametime: %.3fms", timeInfo.deltaTime * 1000.0);
                    ImGui::Text("Update time: %.3fms", timeInfo.updateTime * 1000.0);
                    ImGui::Text("Physics time: %.3fms", timeInfo.simTime);
                    ImGui::Text("Update time without physics: %.3fms", (timeInfo.updateTime * 1000.0) - timeInfo.simTime);
                    ImGui::Text("Framerate: %.1ffps", 1.0 / timeInfo.deltaTime);
                }

                if (ImGui::CollapsingHeader(ICON_FA_BARS u8" Misc")) {
                    ImGui::Text("Frame: %i", timeInfo.frameCounter);
                    ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);
                    ImGui::Text("Current scene: %s (%u)", registry.ctx<SceneInfo>().name.c_str(), registry.ctx<SceneInfo>().id);

                    if (ImGui::Button("Unload Unused Assets")) {
                        renderer->unloadUnusedAssets(registry);
                    }

                    if (ImGui::Button("Reload Materials and Textures")) {
                        renderer->reloadContent(ReloadFlags::Textures | ReloadFlags::Materials);
                    }
                }

                if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" Render Stats")) {
                    const auto& dbgStats = renderer->getDebugStats();
                    auto vkCtx = static_cast<VKRenderer*>(renderer.get())->getHandles();

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

                    if (!enableOpenVR)
                        ImGui::Text("Internal render resolution: %ix%i", screenRTTPass->width, screenRTTPass->height);
                    else
                        ImGui::Text("Internal per-eye render resolution: %ix%i", screenRTTPass->width, screenRTTPass->height);

                    ImGui::Text("Draw calls: %i", dbgStats.numDrawCalls);
                    ImGui::Text("%i pipeline switches", dbgStats.numPipelineSwitches);
                    ImGui::Text("Frustum culled objects: %i", dbgStats.numCulledObjs);
                    ImGui::Text("GPU memory usage: %.3fMB (%.3fMB allocated, %.3fMB available)",
                        (double)totalUsage / 1024.0 / 1024.0,
                        (double)totalBlockBytes / 1024.0 / 1024.0,
                        (double)totalBudget / 1024.0 / 1024.0);
                    ImGui::Text("Active RTT passes: %i/%i", dbgStats.numActiveRTTPasses, dbgStats.numRTTPasses);
                    ImGui::Text("Time spent in renderer: %.3fms", (timeInfo.deltaTime - timeInfo.lastUpdateTime) * 1000.0);
                    ImGui::Text("- Acquiring image: %.3f", dbgStats.imgAcquisitionTime);
                    ImGui::Text("- Waiting for image fence: %.3fms", dbgStats.imgFenceWaitTime);
                    ImGui::Text("- Waiting for command buffer fence: %.3fms", dbgStats.imgFenceWaitTime);
                    ImGui::Text("- Writing command buffer: %.3fms", dbgStats.cmdBufWriteTime);
                    ImGui::Text("GPU render time: %.3fms", renderer->getLastRenderTime() / 1000.0f / 1000.0f);
                    ImGui::Text("V-Sync status: %s", renderer->getVsync() ? "On" : "Off");
                    ImGui::Text("Triangles: %u", dbgStats.numTriangles);
                    ImGui::Text("Lights in view: %i", dbgStats.numLightsInView);
                    ImGui::Text("%i textures loaded", dbgStats.numTexturesLoaded);
                    ImGui::Text("%i materials loaded", dbgStats.numMaterialsLoaded);

                    size_t lightCount = registry.view<WorldLight>().size();
                    size_t worldObjects = registry.view<WorldObject>().size();

                    ImGui::Text("%zu light(s) / %zu world object(s)", lightCount, worldObjects);
                }

                if (ImGui::CollapsingHeader(ICON_FA_MEMORY u8" Memory Stats")) {
                    ImGui::Text("CPU:");
#ifdef CHECK_NEW_DELETE
                    ImGui::Text("Live allocations: %lu", liveAllocations);
                    ImGui::Text("Allocated bytes: %lu", allocatedMem);
#endif
                    ImGui::Separator();
                    ImGui::Text("GPU:");
                    auto vkCtx = static_cast<VKRenderer*>(renderer.get())->getHandles();

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
                    physx::PxScene* scene = physicsSystem->scene();
                    uint32_t nDynamic = scene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC);
                    uint32_t nStatic = scene->getNbActors(physx::PxActorTypeFlag::eRIGID_STATIC);
                    uint32_t nTotal = nDynamic + nStatic;

                    ImGui::Text("%u dynamic actors, %u static actors (%u total)", nDynamic, nStatic, nTotal);
                    uint32_t nConstraints = scene->getNbConstraints();
                    ImGui::Text("%u constraints", nConstraints);
                    uint32_t nShapes = physicsSystem->physics()->getNbShapes();
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
        ZoneScoped;

        if (evtHandler != nullptr && !(runAsEditor && editor->active)) {
            evtHandler->simulate(registry, deltaTime);

            for (auto* system : systems)
                system->simulate(registry, deltaTime);
        }

        if (!runAsEditor || !editor->active) {
            scriptEngine->onSimulate(deltaTime);
        }

        physicsSystem->stepSimulation(deltaTime);
    }

    void WorldsEngine::updateSimulation(float& interpAlpha, double deltaTime) {
        ZoneScoped;
        if (lockSimToRefresh.getInt() || disableSimInterp.getInt() || (editor && editor->active)) {
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
            }

            registry.view<DynamicPhysicsActor>().each([&](auto ent, DynamicPhysicsActor& dpa) {
                currentState[ent] = dpa.actor->getGlobalPose();
            });

            float alpha = simAccumulator / simStepTime.getFloat();

            if (disableSimInterp.getInt())
                alpha = 1.0f;

            registry.view<DynamicPhysicsActor, Transform>().each([&](entt::entity ent, DynamicPhysicsActor& dpa, Transform& transform) {
                if (!previousState.contains(ent)) {
                    transform.position = px2glm(currentState[ent].p);
                    transform.rotation = px2glm(currentState[ent].q);
                } else {
                    transform.position = glm::mix(px2glm(previousState[ent].p), px2glm(currentState[ent].p), (float)alpha);
                    transform.rotation = glm::slerp(px2glm(previousState[ent].q), px2glm(currentState[ent].q), (float)alpha);
                }
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

    bool WorldsEngine::hasCommandLineArg(const char* arg) {
        return EngineArguments::hasArgument(arg);
    }

    WorldsEngine::~WorldsEngine() {
        SDL_WaitThread(renderThreadInstance, nullptr);

        for (auto* system : systems) {
            delete system;
        }

        audioSystem->shutdown(registry);
        if (evtHandler != nullptr && !runAsEditor)
            evtHandler->shutdown(registry);

        registry.clear();

        if (runAsEditor)
            editor.reset();

        if (!dedicatedServer) {
            shutdownRichPresence();

            auto vkCtx = static_cast<VKRenderer*>(renderer.get())->getHandles();
            VKImGUIUtil::destroyObjects(vkCtx);
            renderer.reset();
        }

        PHYSFS_deinit();
        logVrb("Quitting SDL.");
        SDL_Quit();
    }
}
