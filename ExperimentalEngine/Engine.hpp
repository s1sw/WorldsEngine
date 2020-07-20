#pragma once
#include "PCH.hpp"
#include <SDL2/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include "AssetDB.hpp"
#include <unordered_set>
#include <functional>
#include "RenderGraph.hpp"
#ifdef TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"
#endif

struct WorldObject {
	WorldObject(AssetID material, AssetID mesh) 
		: material(material)
		, mesh(mesh)
		, texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f)
		, materialIndex(0) {}

	AssetID material;
	AssetID mesh;
	int materialIndex;
	glm::vec4 texScaleOffset;
};

struct MVP {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

struct VP {
	glm::mat4 view;
	glm::mat4 projection;
};

struct MultiVP {
	glm::mat4 views[8];
	glm::mat4 projections[8];
};

struct PackedLight {
	glm::vec4 pack0;
	glm::vec4 pack1;
	glm::vec4 pack2;
};

enum class LightType {
	Point,
	Spot,
	Directional
};

struct WorldLight {
	WorldLight() : type(LightType::Point), color(1.0f), spotCutoff(1.35f) {}
	WorldLight(LightType type) : type(type), color(1.0f), spotCutoff(1.35f) {}
	LightType type;
	glm::vec3 color;
	float spotCutoff;
};

struct LightUB {
	glm::vec4 pack0;
	glm::mat4 shadowmapMatrix;
	PackedLight lights[16];
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;
	float ao;
};

struct PackedMaterial {
	glm::vec4 pack0;
	glm::vec4 pack1;
};

struct ProceduralObject {
	ProceduralObject() : uploaded(false), readyForUpload(false), visible(true) {}
	AssetID material;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	bool uploaded;
	bool readyForUpload;
	bool visible;
	vku::VertexBuffer vb;
	vku::IndexBuffer ib;
	uint32_t indexCount;
	vk::IndexType indexType;
};

struct Camera {
	Camera() : 
		position(0.0f), 
		rotation(), 
		verticalFOV(1.25f) {}
	glm::vec3 position;
	glm::quat rotation;
	float verticalFOV;

	glm::mat4 getViewMatrix() {
		return glm::lookAt(position, position + (rotation * glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::mat4 getProjectionMatrix(float aspect) {
		return glm::perspective(verticalFOV, aspect, 0.01f, 2500.0f);
	}
};

extern AssetDB g_assetDB;

struct QueueFamilyIndices {
	uint32_t graphics;
	uint32_t present;
};

class Swapchain {
public:
	Swapchain(vk::PhysicalDevice&, vk::Device&, vk::SurfaceKHR&, QueueFamilyIndices qfi, vk::SwapchainKHR oldSwapchain = vk::SwapchainKHR());
	~Swapchain();
	void getSize(uint32_t* x, uint32_t* y) { *x = width; *y = height; }
	vk::Result acquireImage(vk::Device& device, vk::Semaphore semaphore, uint32_t* imageIndex);
	vk::UniqueSwapchainKHR& getSwapchain() { return swapchain; }
	vk::Format imageFormat() { return format; }
	std::vector<vk::Image> images;
	std::vector<vk::ImageView> imageViews;
private:
	vk::Device& device;
	vk::UniqueSwapchainKHR swapchain;
	vk::Format format;
	uint32_t width;
	uint32_t height;
};

typedef uint32_t RenderImageHandle;

struct TextureUsage {
	vk::ImageLayout layout;
	vk::PipelineStageFlagBits stageFlags;
	vk::AccessFlagBits accessFlags;
	RenderImageHandle handle;
};

struct ImageBarrier {
	RenderImageHandle handle;
	vk::ImageLayout oldLayout;
	vk::ImageLayout newLayout;
	vk::ImageAspectFlagBits aspectMask;
	vk::AccessFlagBits srcMask;
	vk::AccessFlagBits dstMask;
	vk::PipelineStageFlagBits srcStage;
	vk::PipelineStageFlagBits dstStage;
};

struct StandardPushConstants {
	glm::vec4 pack0;
	glm::vec4 texScaleOffset;
	// (x: model matrix index, y: material index, z: specular cubemap index)
	glm::ivec4 ubIndices;
};

struct ChunkShadowPushConstants {
	glm::mat4 vp;
	glm::vec4 chunkOffset;
};

struct ModelMatrices {
	glm::mat4 modelMatrices[1024];
};

struct MaterialsUB {
	PackedMaterial materials[256];
};

struct GraphicsSettings {
	GraphicsSettings() : msaaLevel(2), shadowmapRes(1024) {}
	GraphicsSettings(int msaaLevel, int shadowmapRes)
		: msaaLevel(msaaLevel)
		, shadowmapRes(shadowmapRes) {
	}

	int msaaLevel;
	int shadowmapRes;
};

struct Global2DTextureSlot {
	vku::TextureImage2D tex;
	bool present;
};

struct RenderTextureResource {
	vku::GenericImage image;
	vk::ImageAspectFlagBits aspectFlags;
};

struct LoadedMeshData {
	vku::VertexBuffer vb;
	vku::IndexBuffer ib;
	uint32_t indexCount;
	vk::IndexType indexType;
};

struct RenderCtx {
	RenderCtx(
		vk::UniqueCommandBuffer& cmdBuf, 
		entt::registry& reg, 
		uint32_t imageIndex, 
		Camera& cam, 
		std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources, 
		uint32_t width, uint32_t height, 
		std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes)
		: cmdBuf(cmdBuf)
		, reg(reg)
		, imageIndex(imageIndex)
		, cam(cam)
		, rtResources(rtResources)
		, width(width)
		, height(height)
		, loadedMeshes(loadedMeshes) {
	}

	vk::UniqueCommandBuffer& cmdBuf; 
	vk::PipelineCache pipelineCache;

	entt::registry& reg; 
	uint32_t imageIndex;
	Camera& cam;
	Global2DTextureSlot* globalTexArray;
	std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
	std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes;
	uint32_t width, height;
};

struct PassSetupCtx {
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::PipelineCache pipelineCache;
	vk::DescriptorPool descriptorPool;
	// Please only use the pool passed here for immediately executing commands during the setup phase.
	vk::CommandPool commandPool;
	vk::Instance instance;
	VmaAllocator allocator;
	uint32_t graphicsQueueFamilyIdx;
	GraphicsSettings graphicsSettings;
	Global2DTextureSlot* globalTexArray;
	std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
	int swapchainImageCount;
};

class XRInterface;

struct RendererInitInfo {
	SDL_Window* window;
	bool enableVR;
	std::vector<std::string> additionalInstanceExtensions;
	std::vector<std::string> additionalDeviceExtensions;
	XRInterface* xrInterface;
};

class VKRenderer {
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;
	vk::UniqueDevice device;
	vk::UniquePipelineCache pipelineCache;
	vk::UniqueDescriptorPool descriptorPool;
	vk::SurfaceKHR surface;
	std::unique_ptr<Swapchain> swapchain;
	vku::DebugCallback dbgCallback;
	uint32_t graphicsQueueFamilyIdx;
	uint32_t computeQueueFamilyIdx;
	uint32_t presentQueueFamilyIdx;
	uint32_t asyncComputeQueueFamilyIdx;
	uint32_t width, height;
	vk::SampleCountFlagBits msaaSamples;
	int32_t numMSAASamples;
	vk::UniqueRenderPass imguiRenderPass;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::UniqueSemaphore imageAcquire;
	vk::UniqueSemaphore commandComplete;
	vk::UniqueCommandPool commandPool;
	std::vector<vk::UniqueCommandBuffer> cmdBufs;
	std::vector<vk::Fence> cmdBufferFences;
	VmaAllocator allocator;
	
	// stuff related to standard geometry rendering
	RenderImageHandle depthStencilImage;
	RenderImageHandle polyImage;

	// tonemap related stuff
	RenderImageHandle finalPrePresent;

	// shadowmapping stuff
	RenderImageHandle shadowmapImage;

	RenderImageHandle imguiImage;

	std::vector<vk::DescriptorSet> descriptorSets;
	SDL_Window* window;
	vk::UniqueQueryPool queryPool;
	uint64_t lastRenderTimeTicks;
	float timestampPeriod;

	std::unordered_map<RenderImageHandle, RenderTextureResource> rtResources;
	RenderImageHandle lastHandle;

	struct RTResourceCreateInfo {
		vk::ImageCreateInfo ici;
		vk::ImageViewType viewType;
		vk::ImageAspectFlagBits aspectFlags;
	};

	void imageBarrier(vk::CommandBuffer& cb, ImageBarrier& ib);
	RenderImageHandle createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName = nullptr);
	void createSwapchain(vk::SwapchainKHR oldSwapchain);
	void createFramebuffers();
	void createSCDependents();
	void presentNothing(uint32_t imageIndex);
	void loadTex(const char* path, int index);
	void loadAlbedo();
	vku::ShaderModule loadShaderAsset(AssetID id);

	std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
	int frameIdx;
#ifdef TRACY_ENABLE
	std::vector<TracyVkCtx> tracyContexts;
#endif
	Global2DTextureSlot textures[64];
	vku::TextureImageCube cubemaps[64];
	GraphSolver graphSolver;
	uint32_t shadowmapRes;
	bool enableVR;

public:
	double time;
	VKRenderer(RendererInitInfo& initInfo, bool* success);
	void recreateSwapchain();
	void frame(Camera& cam, entt::registry& reg);
	void preloadMesh(AssetID id);
	void uploadProcObj(ProceduralObject& procObj);
	inline float getLastRenderTime() { return lastRenderTimeTicks * timestampPeriod; }

	~VKRenderer();
};
