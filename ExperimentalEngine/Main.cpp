#include "PCH.hpp"
#define SDL_MAIN_HANDLED
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
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#endif
#include "XRInterface.hpp"
#include "SourceModelLoader.hpp"
#include "Editor.hpp"
#include "OpenVRInterface.hpp"
#include "Log.hpp"
#include "Audio.hpp"

AssetDB g_assetDB;

#undef min
#undef max

void setupSDL() {
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO);
}

SDL_cond* sdlEventCV;
SDL_mutex* sdlEventMutex;

struct WindowThreadData {
    bool* runningPtr;
    SDL_Window** windowVarPtr;
};
SDL_Window* window = nullptr;
uint32_t fullscreenToggleEventId;

SDL_Window* createSDLWindow() {
    return SDL_CreateWindow("Worlds Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
    auto ent = reg.create();
    auto& transform = reg.emplace<Transform>(ent, position, rotation);
    transform.scale = scale;
    auto& worldObject = reg.emplace<WorldObject>(ent, 0, meshId);
    worldObject.texScaleOffset = texScaleOffset;
    worldObject.material = materialId;
    return ent;
}

bool useEventThread = false;
int workerThreadOverride = -1;
bool enableXR = false;
bool enableOpenVR = false;
bool runAsEditor = true;
glm::ivec2 windowSize;

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

void engine(char* argv0) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    // Initialisation Stuffs
    // =====================
    setupSDL();
    fullscreenToggleEventId = SDL_RegisterEvents(1);

    InputManager inputManager{window};

    // Ensure that we have a minimum of two workers, as one worker
    // means that jobs can be missed
    JobSystem jobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };

    const char* dataFolder = "EEData";
    const char* dataSrcFolder = "EEDataSrc";
    const char* basePath = SDL_GetBasePath();

    std::string dataStr(basePath);
    dataStr += dataFolder;

    std::string dataSrcStr(basePath);
    dataSrcStr += dataSrcFolder;

    SDL_free((void*)basePath);

    PHYSFS_init(argv0);
    std::cout << "Mounting " << dataStr << "\n";
    PHYSFS_mount(dataStr.c_str(), "/", 0);
    std::cout << "Mounting source " << dataSrcStr << "\n";
    PHYSFS_mount(dataSrcStr.c_str(), "/source", 1);
    PHYSFS_setWriteDir(dataStr.c_str());

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

    setupAudio();

    int frameCounter = 0;

    uint64_t last = SDL_GetPerformanceCounter();

    double deltaTime;
    double currTime = 0.0;
    bool renderInitSuccess = false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

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
    }

    VrApi activeApi = VrApi::None;

    if (enableXR) {
        activeApi = VrApi::OpenXR;
    } else if (enableOpenVR) {
        activeApi = VrApi::OpenVR;
    }

    IVRInterface* vrInterface = enableXR ? (IVRInterface*)&xrInterface : &openvrInterface;

    RendererInitInfo initInfo{ window, additionalInstanceExts, additionalDeviceExts, enableXR || enableOpenVR, activeApi, vrInterface };
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

    entt::entity lHandEnt = createModelObject(registry, glm::vec3(0.0f), glm::quat(), lHandId, devMatId, glm::vec3(0.1f));
    entt::entity rHandEnt = createModelObject(registry, glm::vec3(0.0f), glm::quat(), modelId, devMatId, glm::vec3(0.1f));

    if (!enableOpenVR) {
        registry.destroy(lHandEnt);
        registry.destroy(rHandEnt);
    }

    initPhysx();

    //SDL_SetRelativeMouseMode(SDL_TRUE);
    std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

    Editor editor(registry, inputManager, cam);

    float vrPredictAmount = 0.0f;

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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);

        ImGui::NewFrame();
        inputManager.update();

        uint64_t deltaTicks = now - last;
        last = now;
        deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
        currTime += deltaTime;
        renderer->time = currTime;

        simulate((float)deltaTime);

        SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

        registry.view<DynamicPhysicsActor, Transform>().each([](auto ent, DynamicPhysicsActor& dpa, Transform& transform) {
            auto pose = dpa.actor->getGlobalPose();
            transform.position = px2glm(pose.p);
            transform.rotation = px2glm(pose.q);
        });

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

        if (ImGui::Begin("Info")) {
            ImGui::Text("Frametime: %.3fms", deltaTime * 1000.0);
            ImGui::Text("Framerate: %.3ffps", 1.0 / deltaTime);
            ImGui::Text("GPU render time: %.3fms", renderer->getLastRenderTime() / 1000.0f / 1000.0f);
            ImGui::Text("Frame: %i", frameCounter);
            ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);

            if (enableOpenVR) {
                auto pVRSystem = vr::VRSystem();
                float vsyncToPhoton = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, 
                    vr::Prop_SecondsFromVsyncToPhotons_Float);
                ImGui::Text("VSync to photon latency: %f", vsyncToPhoton);

                float fSecondsSinceLastVsync;
                pVRSystem->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, NULL);

                float fDisplayFrequency = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
                float fFrameDuration = 1.f / fDisplayFrequency;
                float fVsyncToPhotons = pVRSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);

                float fPredictedSecondsFromNow = fFrameDuration - fSecondsSinceLastVsync + fVsyncToPhotons;

                ImGui::Text("Predicted seconds: %f", fPredictedSecondsFromNow + fFrameDuration);
                renderer->setVRPredictAmount(fPredictedSecondsFromNow + fFrameDuration);
            }

            if (ImGui::Button("Unload Unused Assets")) {
                renderer->unloadUnusedMaterials(registry);
            }

            if (ImGui::Button("Reload Materials and Textures")) {
                renderer->reloadMatsAndTextures();
            }
        }
        ImGui::End();

        if (enableOpenVR) {
            openvrInterface.getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, registry.get<Transform>(lHandEnt));
            openvrInterface.getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_RightHand, registry.get<Transform>(rHandEnt));
        }

        std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);
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

        renderer->frame(cam, registry);
        jobSystem.completeFrameJobs();
        frameCounter++;

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        inputManager.endFrame();
    }
    
    delete renderer;
    shutdownPhysx();
    PHYSFS_deinit();
    SDL_CondSignal(sdlEventCV);
    SDL_Quit();
}

int main(int argc, char** argv) {
    cxxopts::Options options("ExpEng", "Experimental game engine");
    options.add_options()("disableEventThread", "Disables processing events on a separate thread.")("workerThreads", "Number of worker threads.");

    auto result = options.parse(argc, argv);
    if (result.count("disableEventThread"))
        useEventThread = false;

#ifndef NDEBUG
    useEventThread = false;
#endif

    if (result.count("workerThreads"))
        workerThreadOverride = result["workerThreads"].as<int>();

    engine(argv[0]);
    return 0;
}