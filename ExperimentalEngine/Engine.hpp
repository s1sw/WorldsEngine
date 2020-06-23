#pragma once
#include <SDL2/SDL.h>
#include "vku/vku.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include "AssetDB.hpp"
#include <entt/entt.hpp>

struct WorldObject {
	WorldObject(AssetID material, AssetID mesh) : material(material), mesh(mesh) {}
	AssetID material;
	AssetID mesh;
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

struct PackedLight {
	glm::vec4 pack0;
	glm::vec4 pack1;
	glm::vec4 pack2;
};

struct LightUB {
	glm::vec4 pack0;
	PackedLight lights[16];
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;
};

struct PackedMaterial {
	glm::vec4 pack0;
	glm::vec4 pack1;
};

struct ProceduralObject {
	ProceduralObject() : uploaded(false), readyForUpload(false), visible(true) {}
	AssetID material;
	// Behind-the-scenes Vulkan stuff handled
	// by procedural mesh creator
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

class VKRenderer {
	struct LoadedMeshData {
		vku::VertexBuffer vb;
		vku::IndexBuffer ib;
		uint32_t indexCount;
		vk::IndexType indexType;
	};

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
	vk::UniqueRenderPass imguiRenderPass;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::UniqueSemaphore imageAcquire;
	vk::UniqueSemaphore commandComplete;
	vk::UniqueCommandPool commandPool;
	std::vector<vk::UniqueCommandBuffer> cmdBufs;
	std::vector<vk::Fence> cmdBufferFences;
	
	// stuff related to standard geometry rendering
	vku::DepthStencilImage depthStencilImage;
	vk::UniqueRenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout dsl;
	vku::UniformBuffer vpUB;
	vku::UniformBuffer lightsUB;
	vku::UniformBuffer materialUB;
	vku::TextureImage2D albedoTex;
	vku::ShaderModule fragmentShader;
	vku::ShaderModule vertexShader;
	vk::UniqueSampler albedoSampler;
	vk::UniqueFramebuffer renderFb;
	vku::GenericImage polyImage;

	// tonemap related stuff
	vku::ShaderModule tonemapShader;
	vk::UniqueDescriptorSetLayout tonemapDsl;
	vk::UniquePipeline tonemapPipeline;
	vk::UniquePipelineLayout tonemapPipelineLayout;
	vk::DescriptorSet tonemapDescriptorSet;
	vku::GenericImage finalPrePresent;
	vk::UniqueFramebuffer finalPrePresentFB;

	std::vector<vk::DescriptorSet> descriptorSets;
	SDL_Window* window;
	vk::UniqueQueryPool queryPool;
	uint64_t lastRenderTimeTicks;
	float timestampPeriod;

	void createSwapchain(vk::SwapchainKHR oldSwapchain);
	void createFramebuffers();
	void createSCDependents();
	void setupTonemapping();
	void setupImGUI();
	void setupStandard();
	void presentNothing(uint32_t imageIndex);
	void loadAlbedo();
	void doTonemap(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex);
	void renderPolys(vk::UniqueCommandBuffer& cmdBuf, entt::registry& reg, uint32_t imageIndex, Camera& cam);
	void updateTonemapDescriptors();

	std::unordered_map<AssetID, LoadedMeshData> loadedMeshes;

public:
	double time;
	VKRenderer(SDL_Window* window, bool* success);
	void recreateSwapchain();
	void frame(Camera& cam, entt::registry& reg);
	void preloadMesh(AssetID id);
	void uploadProcObj(ProceduralObject& procObj);
	inline float getLastRenderTime() { return lastRenderTimeTicks * timestampPeriod; }
	~VKRenderer();
};
