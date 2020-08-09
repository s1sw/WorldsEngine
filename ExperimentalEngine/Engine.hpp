#pragma once
#include "PCH.hpp"
#include <SDL2/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"
#include "AssetDB.hpp"
#include <unordered_set>
#include <functional>
#include "RenderGraph.hpp"
#include "Camera.hpp"
#ifdef TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"
#endif
#include "IVRInterface.hpp"
#include "tracy/TracyVulkan.hpp"
#include "ResourceSlots.hpp"

extern glm::ivec2 windowSize;
class VKRenderer;
class PolyRenderPass;

struct WorldObject {
	WorldObject(AssetID material, AssetID mesh) 
		: material(material)
		, mesh(mesh)
		, texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f)
		, materialIdx(~0u) {}

	AssetID material;
	AssetID mesh;
	glm::vec4 texScaleOffset;

private:
	uint32_t materialIdx;
	friend class VKRenderer;
	friend class PolyRenderPass;
};

struct UseWireframe {};

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
	glm::vec4 viewPos[8];
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

class PolyRenderPass;
class ImGuiRenderPass;

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

struct ModelMatrices {
	glm::mat4 modelMatrices[1024];
};

struct MaterialsUB {
	PackedMaterial materials[256];
};

struct GraphicsSettings {
	GraphicsSettings() : msaaLevel(2), shadowmapRes(1024), enableVr(false) {}
	GraphicsSettings(int msaaLevel, int shadowmapRes, bool enableVr)
		: msaaLevel(msaaLevel)
		, shadowmapRes(shadowmapRes)
		, enableVr(enableVr) {
	}

	int msaaLevel;
	int shadowmapRes;
	bool enableVr;
};

struct Global2DTextureSlot {
	vku::TextureImage2D tex;
	bool present;
};

struct GlobalCubeTextureSlot {
	vku::TextureImageCube tex;
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
		, loadedMeshes(loadedMeshes)
		, enableVR(false)
		, reuploadMats(false) {
	}

	vk::UniqueCommandBuffer& cmdBuf; 
	vk::PipelineCache pipelineCache;

	entt::registry& reg; 
	uint32_t imageIndex;
	Camera& cam;
	std::unique_ptr<TextureSlots>* textureSlots;
	std::unique_ptr<MaterialSlots>* materialSlots;
	bool reuploadMats;
	std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
	std::unordered_map<AssetID, LoadedMeshData>& loadedMeshes;
	uint32_t width, height;
	glm::mat4 vrViewMats[2];
	glm::mat4 vrProjMats[2];
	glm::vec3 viewPos;
	bool enableVR;
#ifdef TRACY_ENABLE
	std::vector<TracyVkCtx>* tracyContexts;
#endif
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
	std::unique_ptr<TextureSlots>* globalTexArray;
	std::unordered_map<RenderImageHandle, RenderTextureResource>& rtResources;
	int swapchainImageCount;
	bool enableVR;
};

// Holds handles to useful Vulkan objects
struct VulkanCtx {
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::PipelineCache pipelineCache;
	vk::DescriptorPool descriptorPool;
	vk::CommandPool commandPool;
	vk::Instance instance;
	VmaAllocator allocator;
	uint32_t graphicsQueueFamilyIdx;
	GraphicsSettings graphicsSettings;
	uint32_t width, height;
	uint32_t renderWidth, renderHeight;
};

class XRInterface;

struct RendererInitInfo {
	SDL_Window* window;
	std::vector<std::string> additionalInstanceExtensions;
	std::vector<std::string> additionalDeviceExtensions;
	bool enableVR;
	VrApi activeVrApi;
	IVRInterface* vrInterface;
};

class VKRenderer {
	const static uint32_t NUM_TEX_SLOTS = 64;
	const static uint32_t NUM_MAT_SLOTS = 256;
	const static uint32_t NUM_CUBEMAP_SLOTS = 64;

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

	
	RenderImageHandle finalPrePresent;
	// openvr doesn't support presenting image layers
	// copy to another image
	RenderImageHandle finalPrePresentR;

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
	void loadTex(const char* path, int index, bool crunch);
	void loadTex(AssetID id, int index);
	void loadCubemap(AssetID id, int index);
	void loadAlbedo();
	vku::ShaderModule loadShaderAsset(AssetID id);
	uint32_t getOrLoadTextureIdx(AssetID textureId);
	uint32_t getOrLoadCubemapIdx(AssetID cubemapId);
	uint32_t getOrLoadMaterialIdx(AssetID materialId);
	void parseMaterial(AssetID matJsonId, PackedMaterial& mat);

	std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;
	int frameIdx;
	std::vector<TracyVkCtx> tracyContexts;
	std::unique_ptr<TextureSlots> texSlots;
	std::unique_ptr<MaterialSlots> matSlots;
	
	GraphSolver graphSolver;
	uint32_t shadowmapRes;
	bool enableVR;
	PolyRenderPass* currentPRP;
	ImGuiRenderPass* irp;
	uint32_t renderWidth, renderHeight;
	IVRInterface* vrInterface;
	VrApi vrApi;
	float vrPredictAmount;
public:
	double time;
	VKRenderer(const RendererInitInfo& initInfo, bool* success);
	void recreateSwapchain();
	void frame(Camera& cam, entt::registry& reg);
	void preloadMesh(AssetID id);
	void uploadProcObj(ProceduralObject& procObj);
	void requestEntityPick();
	void unloadUnusedMaterials(entt::registry& reg);
	void reloadMatsAndTextures();
	bool getPickedEnt(entt::entity* entOut);
	inline float getLastRenderTime() const { return lastRenderTimeTicks * timestampPeriod; }
	void setVRPredictAmount(float amt) { vrPredictAmount = amt; }

	~VKRenderer();
};
