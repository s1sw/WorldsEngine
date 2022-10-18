#pragma once
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <mutex>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkCommandPool)
VK_DEFINE_HANDLE(VkDebugUtilsMessengerEXT)
VK_DEFINE_HANDLE(VkCommandBuffer)
VK_DEFINE_HANDLE(VkSemaphore)
VK_DEFINE_HANDLE(VkFence)
VK_DEFINE_HANDLE(VkDescriptorPool)
#undef VK_DEFINE_HANDLE

struct VkDebugUtilsMessengerCallbackDataEXT;

typedef uint32_t VkBool32;
typedef uint32_t VkFlags;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

namespace R2::VK
{
#define VKCHECK(res) if (res != 0) { printf("RESULT: %i (file %s, line %i)", res, __FILE__, __LINE__); abort(); }
	struct Queues
	{
		uint32_t GraphicsFamilyIndex;
		uint32_t PresentFamilyIndex;
		uint32_t AsyncComputeFamilyIndex;

		VkQueue Graphics;
		VkQueue Present;
		VkQueue AsyncCompute;
	};

	// Commonly used handles that are passed around via a reference to
	// the member struct in the Renderer.
	struct Handles
	{
		VkInstance Instance;
		VkPhysicalDevice PhysicalDevice;
		VkDevice Device;
		Queues Queues;
		VkCommandPool CommandPool;
		VkAllocationCallbacks* AllocCallbacks;
		VmaAllocator Allocator;
		VkDescriptorPool DescriptorPool;
	};

	class Texture;
	struct TextureCreateInfo;

	class Buffer;
	struct BufferCreateInfo;
	
	class Swapchain;
	struct SwapchainCreateInfo;

	class DeletionQueue;
	class CommandBuffer;
	class DescriptorSet;
	class DescriptorSetLayout;

	class IDebugOutputReceiver
	{
	public:
		virtual void DebugMessage(const char* message) = 0;
	};

	struct GraphicsDeviceInfo
	{
		char Name[256];
		float TimestampPeriod;
	};

	class Core
	{
	public:
		Core(IDebugOutputReceiver* dbgOutRecv = nullptr, bool enableValidation = false);

		const GraphicsDeviceInfo& GetDeviceInfo() const;

		Texture* CreateTexture(const TextureCreateInfo& createInfo);
		void DestroyTexture(Texture* tex);

		Buffer* CreateBuffer(const BufferCreateInfo& createInfo);
		Buffer* CreateBuffer(const BufferCreateInfo& createInfo, void* initialData, size_t initialDataSize);
		void DestroyBuffer(Buffer* buf);

		Swapchain* CreateSwapchain(const SwapchainCreateInfo& createInfo);
		void DestroySwapchain(Swapchain* swapchain);

		DescriptorSet* CreateDescriptorSet(DescriptorSetLayout* dsl);
		DescriptorSet* CreateDescriptorSet(DescriptorSetLayout* dsl, uint32_t maxVariableDescriptors);

		void BeginFrame();
		CommandBuffer GetFrameCommandBuffer();
		VkSemaphore GetFrameCompletionSemaphore();
		void QueueBufferUpload(Buffer* buffer, const void* data, uint64_t dataSize, uint64_t dataOffset);
		void QueueBufferToTextureCopy(Buffer* buffer, Texture* texture, uint64_t bufferOffset = 0);
		void QueueTextureUpload(Texture* texture, void* data, uint64_t dataSize, int numMips = -1);
		uint32_t GetFrameIndex() const;
		uint32_t GetNextFrameIndex() const;
		uint32_t GetPreviousFrameIndex() const;
		uint32_t GetNumFramesInFlight() const;
		void EndFrame();

		void WaitIdle();

		~Core();
		const Handles* GetHandles() const;
        IDebugOutputReceiver* GetDebugOutputReceiver();
	private:
		struct BufferUpload
		{
			Buffer* Buffer;
			uint64_t StagingOffset;
			uint64_t DataSize;
			uint64_t DataOffset;
		};

		struct BufferToTextureCopy
		{
			Buffer* Buffer;
			Texture* Texture;
			uint64_t BufferOffset;
			int numMips;
		};

		struct PerFrameResources
		{
			VkCommandBuffer CommandBuffer;
			VkCommandBuffer UploadCommandBuffer;
			VkSemaphore UploadSemaphore;
			VkSemaphore Completion;
			VkFence Fence;
			DeletionQueue* DeletionQueue;
			std::mutex BufferUploadMutex;

			std::vector<BufferUpload> BufferUploads;
			std::vector<BufferToTextureCopy> BufferToTextureCopies;
			uint64_t StagingOffset;
			Buffer* StagingBuffer;
			char* StagingMapped;
		};

		void writeFrameUploadCommands(uint32_t index);

		void setAllocCallbacks();
		void createInstance(bool enableValidation);
		void selectPhysicalDevice();
		void findQueueFamilies();
		bool checkFeatures(VkPhysicalDevice device);
		void createDevice();
		void createCommandPool();
		void createAllocator();
		void createDescriptorPool();

        DeletionQueue* getCurrentDq();

		Handles handles;
        GraphicsDeviceInfo deviceInfo;
		VkDebugUtilsMessengerEXT messenger;
		IDebugOutputReceiver* dbgOutRecv;
		PerFrameResources perFrameResources[2];
		uint32_t frameIndex;
		bool inFrame;
		std::mutex queueMutex;

		friend class Buffer;
		friend class DescriptorSet;
        friend class Event;
		friend class Pipeline;
		friend class Sampler;
		friend class Texture;
		friend class TextureView;
	};
}