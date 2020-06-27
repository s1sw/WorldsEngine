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
#include "VoxelChunkMesher.hpp"
#include "Physics.hpp"
#include "Input.hpp"
#include "ChunkGenerator.hpp"
#include "PhysicsActor.hpp"
#include <execution>

AssetDB g_assetDB;

#undef min
#undef max

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

void createModelObject(entt::registry& reg, glm::vec3 position, glm::quat rotation, AssetID meshId) {
	auto ent = reg.create();
	auto& transform = reg.emplace<Transform>(ent, position, rotation);
	auto& worldObject = reg.emplace<WorldObject>(ent, 0, meshId);
}

void updateChunkCollisions(physx::PxRigidActor* voxelActor, VoxelChunk& voxChunk, physx::PxMaterial* mat) {
	bool presentShapes[16][16][16] = {};

	physx::PxShape** shapes = (physx::PxShape**)std::malloc(voxelActor->getNbShapes() * sizeof(void*));
	int numShapes = voxelActor->getNbShapes();
	voxelActor->getShapes(shapes, numShapes);
	for (int i = 0; i < numShapes; i++) {
		physx::PxShape* shape = shapes[i];
		glm::vec3 localPos = px2glm(shape->getLocalPose().p) - glm::vec3(0.5f);
		if (voxChunk.data[(int)localPos.x][(int)localPos.y][(int)localPos.z] == 0) {
			voxelActor->detachShape(*shape);
			shape->release();
		} else {
			presentShapes[(int)localPos.x][(int)localPos.y][(int)localPos.z] = true;
		}

	}

	for (int x = 0; x < 16; x++)
		for (int y = 0; y < 16; y++)
			for (int z = 0; z < 16; z++) {
				if (voxChunk.data[x][y][z] && !presentShapes[x][y][z]) {
					physx::PxShape* shape = g_physics->createShape(physx::PxBoxGeometry(0.5f, 0.5f, 0.5f), *mat);
					shape->setLocalPose(physx::PxTransform(physx::PxVec3(x + 0.5f, y + 0.5f, z + 0.5f)));
					voxelActor->attachShape(*shape);
				}
			}
}

bool useEventThread = true;
int workerThreadOverride = -1;
extern glm::vec3 shadowOffset;

void engine(char* argv0) {
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
		window = SDL_CreateWindow("ExperimentalEngine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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

	/*AssetID modelId = g_assetDB.addAsset("model.obj");
	AssetID monkeyId = g_assetDB.addAsset("monk.obj");
	renderer.preloadMesh(modelId);
	renderer.preloadMesh(monkeyId);
	createModelObject(registry, glm::vec3(0.0f), glm::quat(), modelId);
	createModelObject(registry, glm::vec3(3.0f, 0.0f, 0.0f), glm::quat(), monkeyId);*/

	initPhysx();

	SDL_SetRelativeMouseMode(SDL_TRUE);
	std::memcpy(reinterpret_cast<void*>(lastState), state, SDL_NUM_SCANCODES);

	physx::PxMaterial* mat = g_physics->createMaterial(0.5f, 0.5f, 0.1f);

	auto& voxJobList = jobSystem.getFreeJobList();
	voxJobList.begin();


	for (int x = -3; x < 3; x++)
		for (int y = -3; y < 3; y++)
			for (int z = -3; z < 3; z++) {
				auto voxEnt = registry.create();
				registry.emplace<Transform>(voxEnt).position = glm::vec3(x * 15.0f, y * 15.0f, z * 15.0f);
				auto& voxChunk = registry.emplace<VoxelChunk>(voxEnt);
				ChunkGenerator().fillChunk(voxChunk, glm::vec3(x * 16.0f, y * 16.0f, z * 16.0f));
				voxChunk.dirty = true;
				auto& procObj = registry.emplace<ProceduralObject>(voxEnt);
				VoxelChunkMesher().updateVoxelChunk(voxChunk, procObj);
				renderer.uploadProcObj(procObj);
				physx::PxRigidActor* voxelActor = static_cast<physx::PxRigidActor*>(g_physics->createRigidStatic(physx::PxTransform(x * 15.0f, y * 15.0f, z * 15.0f)));
				updateChunkCollisions(voxelActor, voxChunk, mat);
				registry.emplace<PhysicsActor>(voxEnt, voxelActor);

				voxelActor->userData = (void*)voxEnt;
				g_scene->addActor(*voxelActor);
			}
	

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

		physx::PxRaycastBuffer hit;
		if (inputManager.mouseButtonPressed(MouseButton::Left)) {
			if (g_scene->raycast(glm2Px(cam.position), glm2Px(cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f)), 10.0f, hit)) {
				auto& voxChunk = registry.get<VoxelChunk>((entt::entity)(uint32_t)hit.block.actor->userData);

				physx::PxTransform localPose = hit.block.shape->getLocalPose();
				glm::vec3 pos = px2glm(localPose.p) - glm::vec3(0.5f);
				pos = glm::floor(pos);

				if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0 && pos.x <= 15 && pos.y <= 15 && pos.z <= 15) {
					voxChunk.data[(int)pos.x][(int)pos.y][(int)pos.z] = 0;
					voxChunk.dirty = true;
					auto& procObj = registry.get<ProceduralObject>((entt::entity)(uint32_t)hit.block.actor->userData);

					updateChunkCollisions(hit.block.actor, voxChunk, mat);


					VoxelChunkMesher().updateVoxelChunk(voxChunk, procObj);
					renderer.uploadProcObj(procObj);
				}
			}
		} else if (inputManager.mouseButtonPressed(MouseButton::Right)) {
			if (g_scene->raycast(glm2Px(cam.position), glm2Px(cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f)), 10.0f, hit)) {
				auto& voxChunk = registry.get<VoxelChunk>((entt::entity)(uint32_t)hit.block.actor->userData);

				physx::PxTransform localPose = hit.block.shape->getLocalPose();
				glm::vec3 pos = px2glm(localPose.p) - glm::vec3(0.5f) + px2glm(hit.block.normal);
				pos = glm::floor(pos);

				if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0 && pos.x <= 15 && pos.y <= 15 && pos.z <= 15) {
					voxChunk.data[(int)pos.x][(int)pos.y][(int)pos.z] = 1;
					voxChunk.dirty = true;
					auto& procObj = registry.get<ProceduralObject>((entt::entity)(uint32_t)hit.block.actor->userData);

					updateChunkCollisions(hit.block.actor, voxChunk, mat);

					VoxelChunkMesher().updateVoxelChunk(voxChunk, procObj);
					renderer.uploadProcObj(procObj);
				}
			}
		}

		static int octaves = 6;
		static float frequency = 1.0f;
		static float lacunarity = 2.0f;
		static float persistence = 0.5f;
		static int seed = 5;
		if (ImGui::Begin("World Generation")) {
			ImGui::DragFloat("Frequency", &frequency);
			ImGui::DragFloat("Persistence", &persistence);
			ImGui::DragFloat("Lacunarity", &lacunarity);
			ImGui::DragInt("Octaves", &octaves);
			ImGui::DragInt("Seed", &seed);

			if (ImGui::Button("Recreate Chunks")) {
				ChunkGenerator cg;
				cg.setFrequency(frequency);
				cg.setOctaveCount(octaves);
				cg.setSeed(seed);
				cg.setLacunarity(lacunarity);
				cg.setPersistence(persistence);

				auto view = registry.view<VoxelChunk, ProceduralObject, Transform>();
				std::for_each(std::execution::par_unseq, view.begin(), view.end(), [&registry, &renderer, view, &cg](entt::entity ent) {
					auto [voxChunk, procObj, tf] = view.get<VoxelChunk, ProceduralObject, Transform>(ent);
					voxChunk.dirty = true;
					cg.fillChunk(voxChunk, (tf.position / 15.0f) * 16.0f);

					VoxelChunkMesher().updateVoxelChunk(voxChunk, procObj);
					procObj.readyForUpload = true;
					procObj.uploaded = false;
				});

				registry.view<ProceduralObject, VoxelChunk, PhysicsActor>().each([&renderer, mat](entt::entity ent, ProceduralObject& procObj, VoxelChunk& voxChunk, PhysicsActor& physAct) {
					updateChunkCollisions(physAct.actor, voxChunk, mat);
					renderer.uploadProcObj(procObj);
				});
			}
		}
		ImGui::End();

		glm::vec3 prevPos = cam.position;
		float moveSpeed = 1.5f;

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

		physx::PxOverlapBuffer overlapBuf{};
		physx::PxQueryFilterData filterData;
		filterData.flags |= physx::PxQueryFlag::eANY_HIT;
		if (g_scene->overlap(physx::PxSphereGeometry(0.05f), physx::PxTransform(cam.position.x, cam.position.y, cam.position.z), overlapBuf, filterData)) {
			cam.position = prevPos;
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

		struct RenderFrameJobData {
			Camera& cam;
			VKRenderer& renderer;
			entt::registry& registry;
		};
		RenderFrameJobData rfjd{ cam, renderer, registry };
		auto& jobList = jobSystem.getFreeJobList();
		jobList.begin();
		jobList.addJob(Job([](void* data) {
			RenderFrameJobData rfjd = *reinterpret_cast<RenderFrameJobData*>(data);
			rfjd.renderer.frame(rfjd.cam, rfjd.registry);
		}, &rfjd));
		jobList.end();
		jobSystem.signalJobListAvailable();
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