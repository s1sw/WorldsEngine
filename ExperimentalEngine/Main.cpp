#include "PCH.hpp"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "JobSystem.hpp"
#include <iostream>
#include <thread>
#include "Engine.hpp"
#include <imgui.h>
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

AssetDB g_assetDB;

#undef min
#undef max

struct DroppedBlockComponent {
	unsigned char blockId;
};

template<typename... Args> void logErr(const char* fmt, Args... args) {
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, args...);
}

template<typename... Args> void logMsg(const char* fmt, Args... args) {
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args...);
}

void setupSDL() {
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO);
}
int playbackSamples;

double dspTime = 0.0;

void audioCallback(void* userData, uint8_t* u8stream, int len) {
	float* stream = reinterpret_cast<float*>(u8stream);
	double sampleLength = 1.0 / 44100.0;

	for (int i = 0; i < len / sizeof(float); i++) {
		//stream[i] = (float)i / (float)len;
		double time = dspTime + (i * sampleLength);
		//stream[i] = sin(time * 2.0 * glm::pi<double>() * 25.0) * 0.2;
	}

	dspTime += (double)(len / 4) / 44100.0;
}

void setupAudio() {
	int numAudioDevices = SDL_GetNumAudioDevices(0);

	if (numAudioDevices == -1) {
		logErr("Failed to enumerate audio devices");
	}

	for (int i = 0; i < numAudioDevices; i++) {
		logMsg("Found audio device: %s", SDL_GetAudioDeviceName(i, 0));
	}

	int numCaptureDevices = SDL_GetNumAudioDevices(1);

	if (numCaptureDevices == -1) {
		logErr("Failed to enumerate capture devices");
	}

	for (int i = 0; i < numCaptureDevices; i++) {
		logMsg("Found capture device: %s", SDL_GetAudioDeviceName(i, 1));
	}

	SDL_AudioSpec desired;
	desired.channels = 1;
	desired.format = AUDIO_F32;
	desired.freq = 44100;
	desired.samples = 1024 * desired.channels;
	desired.callback = audioCallback;

	SDL_AudioSpec obtained;
	int pbDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);

	logMsg("Obtained samples %i", obtained.samples);
	playbackSamples = obtained.samples;

	SDL_PauseAudioDevice(pbDev, 0);
}

SDL_cond* sdlEventCV;
SDL_mutex* sdlEventMutex;

struct WindowThreadData {
	bool* runningPtr;
	SDL_Window** windowVarPtr;
};
SDL_Window* window = nullptr;
uint32_t fullscreenToggleEventId;

// SDL_PollEvent blocks when the window is being resized or moved,
// so I run it on a different thread.
// I would put it through the job system, but thanks to Windows
// weirdness SDL_PollEvent will not work on other threads.
// Thanks Microsoft.
int windowThread(void* data) {
	WindowThreadData* wtd = reinterpret_cast<WindowThreadData*>(data);

	bool* running = wtd->runningPtr;

	window = SDL_CreateWindow("ExperimentalEngine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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

entt::entity createModelObject(entt::registry& reg, glm::vec3 position, glm::quat rotation, AssetID meshId, int materialId, glm::vec3 scale = glm::vec3(1.0f), glm::vec4 texScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)) {
	auto ent = reg.create();
	auto& transform = reg.emplace<Transform>(ent, position, rotation);
	transform.scale = scale;
	auto& worldObject = reg.emplace<WorldObject>(ent, 0, meshId);
	worldObject.texScaleOffset = texScaleOffset;
	worldObject.materialIndex = materialId;
	return ent;
}

bool useEventThread = true;
int workerThreadOverride = -1;
extern glm::vec3 shadowOffset;

template<typename Type>
void clone(const entt::registry& from, entt::registry& to) {
	const auto* data = from.data<Type>();
	const auto size = from.size<Type>();

	if constexpr (ENTT_IS_EMPTY(Type)) {
		to.insert<Type>(data, data + size);
	} else {
		const auto* raw = from.raw<Type>();
		to.insert<Type>(data, data + size, raw, raw + size);
	}
}

void copyRegistry(entt::registry& normalRegistry, entt::registry& renderRegistry) {
	//renderRegistry.clear();
	clone<Transform>(normalRegistry, renderRegistry);
	clone<WorldObject>(normalRegistry, renderRegistry);
	//clone<ProceduralObject>(normalRegistry, renderRegistry);
}

struct EmptyDummyForPerf {};

void engine(char* argv0) {
#ifdef TRACY_ENABLE
	ZoneScoped;
#endif
	// Initialisation Stuffs
	// =====================
	setupSDL();
	fullscreenToggleEventId = SDL_RegisterEvents(1);

	InputManager inputManager{};

	// Ensure that we have a minimum of two workers, as one worker
	// means that jobs can be missed
	JobSystem jobSystem{ workerThreadOverride == -1 ? std::max(SDL_GetCPUCount(), 2) : workerThreadOverride };

	// Janky C string juggling
	const char* dataFolder = "EEData";
	char* dataPath = SDL_GetBasePath();
	size_t dataStrLen = strlen(dataPath) + 1 + strlen(dataFolder);
	char* dataStr = (char*)malloc(dataStrLen);
	strcpy_s(dataStr, dataStrLen, dataPath);
	strcat_s(dataStr, dataStrLen, dataFolder);
	SDL_free(dataPath);

	std::cout << "Mounting " << dataStr << "\n";
	PHYSFS_init(argv0);
	PHYSFS_mount(dataStr, "/", 0);

	//g_assetDB.addAsset("testTex.png");

	bool running = true;

	if (useEventThread) {
		sdlEventCV = SDL_CreateCond();
		sdlEventMutex = SDL_CreateMutex();

		WindowThreadData wtd{ &running, &window };
		SDL_DetachThread(SDL_CreateThread(windowThread, "Window Thread", &wtd));
		SDL_Delay(1000);
	} else {
		window = SDL_CreateWindow(
			"ExperimentalEngine", 
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
			1280, 720, 
			SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(window);
	VKRenderer renderer(window, &renderInitSuccess);

	if (!renderInitSuccess) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to initialise renderer", window);
		return;
	}

	Camera cam{};
	cam.position = glm::vec3(0.0f, 0.0f, -1.0f);
	const Uint8* state = SDL_GetKeyboardState(NULL);
	Uint8 lastState[SDL_NUM_SCANCODES];

	float lookX = 0.0f;
	float lookY = 0.0f;

	entt::registry registry;

	AssetID modelId = g_assetDB.addAsset("model.obj");
	AssetID monkeyId = g_assetDB.addAsset("monk.obj");
	renderer.preloadMesh(modelId);
	renderer.preloadMesh(monkeyId);
	entt::entity boxEnt = createModelObject(registry, glm::vec3(0.0f, -2.0f, 0.0f), glm::quat(), modelId, 0, glm::vec3(5.0f, 1.0f, 5.0f));

	createModelObject(registry, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), monkeyId, 1);

	AssetID droppedObjectID = g_assetDB.addAsset("droppeditem.obj");
	renderer.preloadMesh(droppedObjectID);

	initPhysx();

	SDL_SetRelativeMouseMode(SDL_TRUE);
	std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

	while (running) {
		uint64_t now = SDL_GetPerformanceCounter();
		if (!useEventThread) {
			SDL_Event evt;
			while (SDL_PollEvent(&evt)) {
				if (evt.type == SDL_QUIT) {
					running = false;
					break;
				}
				if (ImGui::GetCurrentContext())
					ImGui_ImplSDL2_ProcessEvent(&evt);
			}
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);

		ImGui::NewFrame();
		inputManager.update(window);

		uint64_t deltaTicks = now - last;
		last = now;
		deltaTime = deltaTicks / (double)SDL_GetPerformanceFrequency();
		currTime += deltaTime;
		renderer.time = currTime;

		simulate((float)deltaTime);

		registry.view<DynamicPhysicsActor, Transform>().each([](auto ent, DynamicPhysicsActor& dpa, Transform& transform) {
			auto pose = dpa.actor->getGlobalPose();
			transform.position = px2glm(pose.p);
			transform.rotation = px2glm(pose.q);
		});

		glm::vec3 prevPos = cam.position;
		float moveSpeed = 5.0f;

		if (state[SDL_SCANCODE_LSHIFT])
			moveSpeed *= 2.0f;

		if (state[SDL_SCANCODE_W]) {
			cam.position += cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
		}

		if (state[SDL_SCANCODE_S]) {
			cam.position -= cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
		}

		if (state[SDL_SCANCODE_A]) {
			cam.position += cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
		}

		if (state[SDL_SCANCODE_D]) {
			cam.position -= cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
		}

		if (state[SDL_SCANCODE_SPACE]) {
			cam.position += cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
		}

		if (state[SDL_SCANCODE_LCTRL]) {
			cam.position -= cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
		}
		
		{
			physx::PxOverlapBuffer overlapBuf{};
			physx::PxQueryFilterData filterData;
			filterData.flags |= physx::PxQueryFlag::eANY_HIT;
			if (g_scene->overlap(physx::PxSphereGeometry(0.05f), physx::PxTransform(cam.position.x, cam.position.y, cam.position.z), overlapBuf, filterData)) {
				cam.position = prevPos;
			}
		}

		if (state[SDL_SCANCODE_RCTRL] && !lastState[SDL_SCANCODE_RCTRL]) {
			SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
		}

		if (state[SDL_SCANCODE_F3] && !lastState[SDL_SCANCODE_F3]) {
			renderer.recreateSwapchain();
		}

		if (state[SDL_SCANCODE_F11] && !lastState[SDL_SCANCODE_F11]) {
			SDL_Event evt;
			SDL_zero(evt);
			evt.type = fullscreenToggleEventId;
			SDL_PushEvent(&evt);
		}

		if (ImGui::Begin("Info")) {
			ImGui::Text("Frametime: %.3fms", deltaTime * 1000.0);
			ImGui::Text("Framerate: %.3ffps", 1.0 / deltaTime);
			ImGui::Text("GPU render time: %.3fms", renderer.getLastRenderTime() / 1000.0f / 1000.0f);
			ImGui::Text("Frame: %i", frameCounter);
			ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);
			ImGui::DragFloat3("Shadow Offset", &shadowOffset.x);
		}
		ImGui::End();

		int x, y;

		SDL_GetRelativeMouseState(&x, &y);

		lookX += (float)x * 0.005f;
		lookY += (float)y * 0.005f;

		lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

		cam.rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));

		std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);
		ImGui::Render();

		if (useEventThread) {
			SDL_LockMutex(sdlEventMutex);
			SDL_CondSignal(sdlEventCV);
			SDL_UnlockMutex(sdlEventMutex);
		}

		glm::vec3 camPos = cam.position;

		registry.sort<ProceduralObject>([&registry, &camPos](entt::entity a, entt::entity b) -> bool {
			auto& aTransform = registry.get<Transform>(a);
			auto& bTransform = registry.get<Transform>(b);
			return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, aTransform.position);
		}, entt::insertion_sort{});

		registry.sort<WorldObject>([&registry, &camPos](entt::entity a, entt::entity b) -> bool {
			auto& aTransform = registry.get<Transform>(a);
			auto& bTransform = registry.get<Transform>(b);
			return glm::distance2(camPos, aTransform.position) < glm::distance2(camPos, aTransform.position);
			}, entt::insertion_sort{});

		struct RenderFrameJobData {
			Camera& cam;
			VKRenderer& renderer;
			entt::registry& registry;
		};

		renderer.frame(cam, registry);
		jobSystem.completeFrameJobs();
		frameCounter++;
	}
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

	if (result.count("workerThreads"))
		workerThreadOverride = result["workerThreads"].as<int>();

	engine(argv[0]);
	return 0;
}