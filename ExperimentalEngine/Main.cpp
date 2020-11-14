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
    bool runAsEditor = false;
    bool pauseSim = false;
    glm::ivec2 windowSize;
    SceneInfo currentScene;
    IGameEventHandler* evtHandler;

    SDL_TimerID presenceUpdateTimer;
    void onDiscordReady(const DiscordUser* user) {
        logMsg("Rich presence ready for %s", user->username);

        presenceUpdateTimer = SDL_AddTimer(1000, [](uint32_t interval, void*) {
            std::string state = ((runAsEditor ? "Editing " : "On ") + currentScene.name);
#ifndef NDEBUG
            state += "(DEVELOPMENT BUILD)";
#endif
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

    SDL_Surface* loadDataFileToSurface(std::string fName) {
        int width, height, channels;

        std::string basePath = SDL_GetBasePath();
        basePath += "EEData";
#ifdef _WIN32
        basePath += '\\';
#else
        basePath += '/';
#endif
        basePath += fName;
        unsigned char* imgDat = stbi_load(basePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

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
        return SDL_CreateRGBSurfaceFrom((void*)imgDat, width, height, 32, 4 * width, rmask, gmask, bmask, amask);
    }

    void setWindowIcon(SDL_Window* win) {
        auto surf = loadDataFileToSurface("icon.png");
        SDL_SetWindowIcon(win, surf);
        SDL_FreeSurface(surf);
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

    struct SplashWindow {
        SDL_Window* win;
        SDL_Renderer* renderer;
        SDL_Surface* bgSurface;
        SDL_Texture* bgTexture;
    };

    void destroySplashWindow(SplashWindow splash) {
        SDL_DestroyTexture(splash.bgTexture);
        SDL_DestroyRenderer(splash.renderer);
        SDL_FreeSurface(splash.bgSurface);
        SDL_DestroyWindow(splash.win);
    }

    void redrawSplashWindow(SplashWindow splash, std::string overlay) {
        SDL_PumpEvents();

        SDL_RenderClear(splash.renderer);
        SDL_RenderCopy(splash.renderer, splash.bgTexture, nullptr, nullptr);

        if (!overlay.empty()) {
            SDL_Surface* s = loadDataFileToSurface("SplashText/" + overlay + ".png");
            SDL_Texture* t = SDL_CreateTextureFromSurface(splash.renderer, s);

            SDL_Rect targetRect;
            targetRect.x = 544;
            targetRect.y = 546;
            targetRect.w = 256;
            targetRect.h = 54;

            SDL_RenderCopy(splash.renderer, t, nullptr, &targetRect);

            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }

        SDL_RenderPresent(splash.renderer);
    }

    SplashWindow createSplashWindow() {
        SplashWindow splash;

        int nRenderDrivers = SDL_GetNumRenderDrivers();
        int driverIdx = -1;

        for (int i = 0; i < nRenderDrivers; i++) {
            SDL_RendererInfo inf;
            SDL_GetRenderDriverInfo(i, &inf);

            logMsg("Render driver: %s", inf.name);

#ifdef _WIN32
            if (strcmp(inf.name, "direct3d11") == 0)
                driverIdx = i;
#endif
        }

        splash.win = SDL_CreateWindow("Loading...", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            800, 600, 
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_SKIP_TASKBAR
        );

        if (splash.win == nullptr) {
            fatalErr("Failed to create splash screen");
        }

        splash.renderer = SDL_CreateRenderer(splash.win, driverIdx, SDL_RENDERER_ACCELERATED);

        if (splash.renderer == nullptr) {
            fatalErr("Failed to create splash screen renderer");
        }

        SDL_RaiseWindow(splash.win);

        splash.bgSurface = loadDataFileToSurface("splash.png");
        splash.bgTexture = SDL_CreateTextureFromSurface(splash.renderer, splash.bgSurface);
        setWindowIcon(splash.win);

        return splash;
    }

    JobSystem* g_jobSys;
    double simAccumulator = 0.0;

    std::unordered_map<entt::entity, physx::PxTransform> currentState;
    std::unordered_map<entt::entity, physx::PxTransform> previousState;
    extern std::function<void(entt::registry&)> onSceneLoad;

    void setupPhysfs(char* argv0) {
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

    void engine(char* argv0) {
        ZoneScoped;
        // Initialisation Stuffs
        // =====================
        setupSDL();

        Console console;

        auto splashWindow = createSplashWindow();
        redrawSplashWindow(splashWindow, "");

        setupPhysfs(argv0);
        redrawSplashWindow(splashWindow, "starting up");

        fullscreenToggleEventId = SDL_RegisterEvents(1);

        InputManager inputManager{ window };

        // Ensure that we have a minimum of two workers, as one worker
        // means that jobs can be missed
        JobSystem jobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };
        g_jobSys = &jobSystem;

        currentScene.name = "";

        redrawSplashWindow(splashWindow, "loading assetdb");
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

        setWindowIcon(window);

        int frameCounter = 0;

        uint64_t last = SDL_GetPerformanceCounter();

        double deltaTime;
        double currTime = 0.0;
        double lastUpdateTime = 0.0;
        bool renderInitSuccess = false;

        redrawSplashWindow(splashWindow, "initialising ui");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.IniFilename = runAsEditor ? "imgui_editor.ini" : "imgui.ini";
        // Disabling this for now as it seems to cause random freezes
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        io.Fonts->TexDesiredWidth = 512.f;

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

        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.079f, 0.076f, 0.090f, 1.000f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.31f, 0.29f, 0.37f, 0.40f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.41f, 0.39f, 0.50f, 0.40f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.41f, 0.40f, 0.50f, 0.62f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.26f, 0.35f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.19f, 0.24f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.56f, 0.77f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.56f, 0.54f, 0.66f, 0.40f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.76f, 0.73f, 0.88f, 0.40f);
        colors[ImGuiCol_Button] = ImVec4(0.31f, 0.29f, 0.37f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.47f, 0.45f, 0.57f, 0.40f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.21f, 0.20f, 0.26f, 0.40f);
        colors[ImGuiCol_Header] = ImVec4(0.31f, 0.29f, 0.37f, 0.40f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.47f, 0.45f, 0.57f, 0.40f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.21f, 0.20f, 0.25f, 0.40f);
        colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.45f, 0.57f, 0.74f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.59f, 0.57f, 0.71f, 0.74f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.35f, 0.33f, 0.41f, 0.74f);
        colors[ImGuiCol_Tab] = ImVec4(0.456f, 0.439f, 0.541f, 0.400f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.51f, 0.49f, 0.62f, 0.40f);
        colors[ImGuiCol_TabActive] = ImVec4(0.56f, 0.54f, 0.71f, 0.40f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.27f, 0.26f, 0.32f, 0.40f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.42f, 0.39f, 0.57f, 0.40f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.58f, 0.54f, 0.80f, 0.78f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        ImGui::GetStyle().WindowBorderSize = 0.0f;
        ImGui::GetStyle().PopupBorderSize = 0.0f;
        ImGui::GetStyle().FrameRounding = 2.0f;
        ImGui::GetStyle().PopupRounding = 1.0f;
        ImGui::GetStyle().ScrollbarRounding = 3.0f;
        ImGui::GetStyle().GrabRounding = 2.0f;
        ImGui::GetStyle().ChildBorderSize = 0.0f;

        ImGui_ImplSDL2_InitForVulkan(window);

        std::vector<std::string> additionalInstanceExts;
        std::vector<std::string> additionalDeviceExts;

        OpenVRInterface openvrInterface;

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

        RendererInitInfo initInfo{ window, additionalInstanceExts, additionalDeviceExts, enableOpenVR, activeApi, vrInterface, runAsEditor, "Converge" };
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
        createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, grassMatId, glm::vec3(5.0f, 1.0f, 5.0f));

        createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, devMatId);

        entt::entity dirLightEnt = registry.create();
        registry.emplace<WorldLight>(dirLightEnt, LightType::Directional);
        registry.emplace<Transform>(dirLightEnt, glm::vec3(0.0f), glm::angleAxis(glm::radians(90.01f), glm::vec3(1.0f, 0.0f, 0.0f)));

        AssetID lHandId = g_assetDB.addOrGetExisting("lhand.obj");

        renderer->preloadMesh(lHandId);

        initPhysx(registry);

        //SDL_SetRelativeMouseMode(SDL_TRUE);
        std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

        EngineInterfaces interfaces{
                .vrInterface = enableOpenVR ? &openvrInterface : nullptr,
                .renderer = renderer,
                .mainCamera = &cam,
                .inputManager = &inputManager
        };

        auto vkCtx = renderer->getVKCtx();
        VKImGUIUtil::createObjects(vkCtx);

        redrawSplashWindow(splashWindow, "initialising editor");

        Editor editor(registry, interfaces);

        if (!runAsEditor)
            pauseSim = false;

        initRichPresence();

        console.registerCommand(cmdLoadScene, "scene", "Loads a scene.", &registry);
        console.registerCommand(cmdToggleFullscreen, "toggleFullscreen", "Toggles fullscreen.", nullptr);
        console.registerCommand([&](void*, const char*) {
            runAsEditor = false;
            pauseSim = false;
            evtHandler->onSceneStart(registry);
            registry.view<AudioSource>().each([](auto ent, auto& as) {
                if (as.playOnSceneOpen) {
                    as.isPlaying = true;
                }
                });
            renderer->reloadMatsAndTextures();
            }, "play", "play.", nullptr);

        console.registerCommand([&](void*, const char*) {
            runAsEditor = true;
            pauseSim = true;
            renderer->reloadMatsAndTextures();
            }, "pauseAndEdit", "pause and edit.", nullptr);

        console.registerCommand([&](void*, const char*) {
            runAsEditor = true;
            loadScene(currentScene.id, registry);
            pauseSim = true;
            renderer->reloadMatsAndTextures();
            }, "reloadAndEdit", "reload and edit.", nullptr);

        console.registerCommand([&](void*, const char*) {
            runAsEditor = false;
            pauseSim = false;
            renderer->reloadMatsAndTextures();
            }, "unpause", "unpause and go back to play mode.", nullptr);

        ConVar showDebugInfo("showDebugInfo", "1", "Shows the debug info window");
        ConVar lockSimToRefresh("sim_lockToRefresh", "0", "Instead of using a simulation timestep, run the simulation in lockstep with the rendering.");
        ConVar disableSimInterp("sim_disableInterp", "0", "Disables interpolation and uses the results of the last run simulation step.");
        ConVar simStepTime("sim_stepTime", "0.01");

        if (runAsEditor)
            disableSimInterp.setValue("1");

        if (enableOpenVR) {
            lockSimToRefresh.setValue("1");
            disableSimInterp.setValue("1");
        }

        if (!runAsEditor && PHYSFS_exists("CommandScripts/startup.txt"))
            console.executeCommandStr("exec CommandScripts/startup");

        if (evtHandler != nullptr) {

            evtHandler->init(registry, interfaces);

            if (!runAsEditor)
                evtHandler->onSceneStart(registry);
        }

        onSceneLoad = [](entt::registry& reg) {
            if (evtHandler && !runAsEditor) {
                evtHandler->onSceneStart(reg);
            }
        };

        uint32_t w, h;

        if (enableOpenVR) {
            openvrInterface.getRenderResolution(&w, &h);
        } else {
            w = 1600;
            h = 900;
        }

        RTTPassHandle screenRTTPass;
        RTTPassCreateInfo screenRTTCI;
        screenRTTCI.enableShadows = true;
        screenRTTCI.width = w;
        screenRTTCI.height = h;
        screenRTTCI.isVr = enableOpenVR;
        screenRTTCI.outputToScreen = true;
        screenRTTCI.useForPicking = false;
        screenRTTPass = renderer->createRTTPass(screenRTTCI);

        redrawSplashWindow(splashWindow, "initialising audio");
        AudioSystem as;
        as.initialise(registry);

        SDL_ShowWindow(window);
        destroySplashWindow(splashWindow);

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
            inputManager.update();

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

            if (evtHandler != nullptr && !runAsEditor)
                evtHandler->preSimUpdate(registry, deltaTime);

            double simTime = 0.0;
            if (!pauseSim) {
                PerfTimer perfTimer;
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

            editor.setActive(runAsEditor);
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

            uint64_t updateEnd = SDL_GetPerformanceCounter();

            uint64_t updateLength = updateEnd - updateStart;
            double updateTime = updateLength / (double)SDL_GetPerformanceFrequency();

            if (showDebugInfo.getInt()) {
                bool open = true;
                if (ImGui::Begin("Info", &open)) {
                    static float historicalFrametimes[128] = { 0.0f };
                    static int historicalFrametimeIdx = 0;

                    historicalFrametimes[historicalFrametimeIdx] = deltaTime * 1000.0;
                    historicalFrametimeIdx++;
                    if (historicalFrametimeIdx >= 128) {
                        historicalFrametimeIdx = 0;
                    }

                    if (ImGui::CollapsingHeader(ICON_FA_CLOCK u8" Performance")) {
                        ImGui::PlotLines("Historical Frametimes", historicalFrametimes, 128, historicalFrametimeIdx, nullptr, 0.0f, 20.0f, ImVec2(0.0f, 125.0f));
                        ImGui::Text("Frametime: %.3fms", deltaTime * 1000.0);
                        ImGui::Text("Update time: %.3fms", updateTime * 1000.0);
                        ImGui::Text("Physics time: %.3fms", simTime);
                        ImGui::Text("Update time without physics: %.3fms", (updateTime * 1000.0) - simTime);
                        ImGui::Text("Framerate: %.1ffps", 1.0 / deltaTime);
                    }

                    if (ImGui::CollapsingHeader(ICON_FA_BARS u8" Misc")) {
                        ImGui::Text("Frame: %i", frameCounter);
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
                        ImGui::Text("Time spent in renderer: %.3fms", (deltaTime - lastUpdateTime) * 1000.0);
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

            std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

            as.update(registry, cam.position, cam.rotation);

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
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position);
                }, entt::insertion_sort{});

            registry.sort<WorldObject>([&registry, &camPos](entt::entity a, entt::entity b) {
                auto& aTransform = registry.get<Transform>(a);
                auto& bTransform = registry.get<Transform>(b);
                return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, bTransform.position) || registry.has<UseWireframe>(a);
                }, entt::insertion_sort{});

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            renderer->frame(cam, registry);
            jobSystem.completeFrameJobs();
            frameCounter++;

            inputManager.endFrame();

            lastUpdateTime = updateTime;

            if (recreateScreenRTT) {
                int newWidth;
                int newHeight;

                SDL_GetWindowSize(window, &newWidth, &newHeight);

                if (enableOpenVR) {
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

        if (evtHandler != nullptr && !runAsEditor)
            evtHandler->shutdown(registry);

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

    void initEngine(EngineInitOptions initOptions, char* argv0) {
        useEventThread = initOptions.useEventThread;
        workerThreadOverride = initOptions.workerThreadOverride;
        evtHandler = initOptions.eventHandler;
        runAsEditor = initOptions.runAsEditor;
        enableOpenVR = initOptions.enableVR;
        engine(argv0);
    }
}