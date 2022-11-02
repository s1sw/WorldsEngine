#include <R2/VKCore.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKSwapchain.hpp>
#include <R2/VKUtil.hpp>
#include <R2/VKDeletionQueue.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <string.h>

size_t operator""_KB(size_t sz)
{
	return sz * 1000;
}

size_t operator""_MB(size_t sz)
{
	return sz * 1000 * 1000;
}

extern R2::VK::IDebugOutputReceiver* vmaDebugOutputRecv;

namespace R2::VK
{
	const uint32_t NUM_FRAMES_IN_FLIGHT = 2;
	const size_t STAGING_BUFFER_SIZE = 64_MB;

	Core::Core(IDebugOutputReceiver* dbgOutRecv, bool enableValidation, const char** instanceExts, const char** deviceExts)
		: inFrame(false)
		, frameIndex(0)
	{
		vmaDebugOutputRecv = dbgOutRecv;

		setAllocCallbacks();
		createInstance(enableValidation, instanceExts);
		findQueueFamilies();
		createDevice(deviceExts);
		createCommandPool();
		createAllocator();
		createDescriptorPool();

        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(handles.PhysicalDevice, &deviceProps);

        strncpy(deviceInfo.Name, deviceProps.deviceName, 256);
        deviceInfo.TimestampPeriod = deviceProps.limits.timestampPeriod;

		Utils::SetupImmediateCommandBuffer(GetHandles());

		for (uint32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cbai.commandBufferCount = 1;
			cbai.commandPool = handles.CommandPool;
			cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

			VKCHECK(vkAllocateCommandBuffers(handles.Device, &cbai, &perFrameResources[i].CommandBuffer));
			VKCHECK(vkAllocateCommandBuffers(handles.Device, &cbai, &perFrameResources[i].UploadCommandBuffer));

			VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VKCHECK(vkCreateSemaphore(handles.Device, &sci, handles.AllocCallbacks, &perFrameResources[i].Completion));
			VKCHECK(vkCreateSemaphore(handles.Device, &sci, handles.AllocCallbacks, &perFrameResources[i].UploadSemaphore));

			VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VKCHECK(vkCreateFence(handles.Device, &fci, handles.AllocCallbacks, &perFrameResources[i].Fence));

			perFrameResources[i].DeletionQueue = new DeletionQueue(GetHandles());

			BufferCreateInfo stagingCreateInfo{};
			// 8MB staging buffer
			stagingCreateInfo.Size = STAGING_BUFFER_SIZE;
			stagingCreateInfo.Usage = BufferUsage::Storage;
			stagingCreateInfo.Mappable = true;
			perFrameResources[i].StagingBuffer = CreateBuffer(stagingCreateInfo);

			perFrameResources[i].StagingMapped = (char*)perFrameResources[i].StagingBuffer->Map();
		}

		this->dbgOutRecv = dbgOutRecv;
	}

	const GraphicsDeviceInfo& Core::GetDeviceInfo() const
	{
		return deviceInfo;
	}

	Texture* Core::CreateTexture(const TextureCreateInfo& createInfo)
	{
		return new Texture(this, createInfo);
	}

	void Core::DestroyTexture(Texture* t)
	{
		delete static_cast<Texture*>(t);
	}

	Buffer* Core::CreateBuffer(const BufferCreateInfo& createInfo)
	{
		return new Buffer(this, createInfo);
	}

	Buffer* Core::CreateBuffer(const BufferCreateInfo& create, void* initialData, size_t initialDataSize)
	{
		Buffer* b = CreateBuffer(create);
		QueueBufferUpload(b, initialData, initialDataSize, 0);

		return b;
	}

	void Core::DestroyBuffer(Buffer* b)
	{
		delete b;
	}

	Swapchain* Core::CreateSwapchain(const SwapchainCreateInfo& createInfo)
	{
		return new Swapchain(this, createInfo);
	}

	void Core::DestroySwapchain(Swapchain* swapchain)
	{
		delete swapchain;
	}

	DescriptorSet* Core::CreateDescriptorSet(DescriptorSetLayout* dsl)
	{
		VkDescriptorSet ds;
		VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		dsai.descriptorSetCount = 1;
		VkDescriptorSetLayout vdsl = dsl->GetNativeHandle();
		dsai.pSetLayouts = &vdsl;
		dsai.descriptorPool = handles.DescriptorPool;

		VKCHECK(vkAllocateDescriptorSets(handles.Device, &dsai, &ds));
		return new DescriptorSet(this, ds);
	}

	DescriptorSet* Core::CreateDescriptorSet(DescriptorSetLayout* dsl, uint32_t maxVariableDescriptors)
	{
		VkDescriptorSet ds;
		VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
		variableCountInfo.descriptorSetCount = 1;
		variableCountInfo.pDescriptorCounts = &maxVariableDescriptors;

		VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		dsai.descriptorSetCount = 1;
		VkDescriptorSetLayout vdsl = dsl->GetNativeHandle();
		dsai.pSetLayouts = &vdsl;
		dsai.descriptorPool = handles.DescriptorPool;
		dsai.pNext = &variableCountInfo;

		VKCHECK(vkAllocateDescriptorSets(handles.Device, &dsai, &ds));
		return new DescriptorSet(this, ds);
	}

	// Gets the index of the last frame. Loops back round on frame 0
	int getPreviousFrameIndex(int current)
	{
		int v = current - 1;

		if (v == -1)
		{
			v += NUM_FRAMES_IN_FLIGHT;
		}

		return v;
	}

	int getNextFrameIndex(int current)
	{
		return (current + 1) % NUM_FRAMES_IN_FLIGHT;
	}

	void Core::BeginFrame()
	{
		inFrame = true;
		frameIndex++;

		if (frameIndex >= NUM_FRAMES_IN_FLIGHT)
		{
			frameIndex = 0;
		}

		PerFrameResources& frameResources = perFrameResources[frameIndex];

		VKCHECK(vkWaitForFences(handles.Device, 1, &frameResources.Fence, VK_TRUE, UINT64_MAX));
		VKCHECK(vkResetFences(handles.Device, 1, &frameResources.Fence));

		// Prepare the command buffer for recording
		VKCHECK(vkResetCommandBuffer(frameResources.CommandBuffer, 0));

		// Now we know that the command buffer has finished executing, so we can
		// go through the deletion queue and clean up
		frameResources.DeletionQueue->Cleanup();

		VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VKCHECK(vkBeginCommandBuffer(frameResources.CommandBuffer, &cbbi));
	}

	CommandBuffer Core::GetFrameCommandBuffer()
	{
		return CommandBuffer(perFrameResources[frameIndex].CommandBuffer);
	}

	VkSemaphore Core::GetFrameCompletionSemaphore()
	{
		return perFrameResources[frameIndex].Completion;
	}

	void Core::QueueBufferUpload(Buffer* buffer, const void* data, uint64_t dataSize, uint64_t dataOffset)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		std::unique_lock buLock{frameResources.BufferUploadMutex};

		if (dataSize >= STAGING_BUFFER_SIZE)
		{
			this->dbgOutRecv->DebugMessage("Queued buffer too big to go in staging buffer! THIS IS A STALL");
			VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bci.size = dataSize;
			bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			VmaAllocationCreateInfo vaci{};
			vaci.usage = VMA_MEMORY_USAGE_AUTO;
			vaci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			VkBuffer tempBuffer;
			VmaAllocation tempAlloc;
			VmaAllocationInfo tempAllocInfo{};
			VKCHECK(vmaCreateBuffer(handles.Allocator, &bci, &vaci, &tempBuffer, &tempAlloc, &tempAllocInfo));

			memcpy(tempAllocInfo.pMappedData, data, dataSize);

			std::unique_lock queueLock{queueMutex};
			VKCHECK(vkResetCommandBuffer(frameResources.UploadCommandBuffer, 0));

			// Record upload command buffer
			VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VKCHECK(vkBeginCommandBuffer(frameResources.UploadCommandBuffer, &cbbi));

			VkBufferCopy bc{};
			bc.size = dataSize;
			bc.dstOffset = dataOffset;
			vkCmdCopyBuffer(frameResources.UploadCommandBuffer, tempBuffer, buffer->GetNativeHandle(), 1, &bc);

			VKCHECK(vkEndCommandBuffer(frameResources.UploadCommandBuffer));

			VkSubmitInfo uploadSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

			uploadSubmitInfo.commandBufferCount = 1;
			uploadSubmitInfo.pCommandBuffers = &frameResources.UploadCommandBuffer;

			VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &uploadSubmitInfo, VK_NULL_HANDLE));
			WaitIdle();

			vmaDestroyBuffer(handles.Allocator, tempBuffer, tempAlloc);

			return;
		}

		if (dataSize + frameResources.StagingOffset >= STAGING_BUFFER_SIZE)
		{
			std::unique_lock queueLock{queueMutex};
			this->dbgOutRecv->DebugMessage("Flushing staged uploads!!! THIS IS A STALL");
			writeFrameUploadCommands(frameIndex);

			VkSubmitInfo uploadSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

			uploadSubmitInfo.commandBufferCount = 1;
			uploadSubmitInfo.pCommandBuffers = &frameResources.UploadCommandBuffer;

			VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &uploadSubmitInfo, VK_NULL_HANDLE));
			WaitIdle();
		}

		memcpy(frameResources.StagingMapped + frameResources.StagingOffset, data, dataSize);

		frameResources.BufferUploads.emplace_back(buffer, frameResources.StagingOffset, dataSize, dataOffset);
		frameResources.StagingOffset += dataSize;
	}

	void Core::QueueBufferToTextureCopy(Buffer* buffer, Texture* texture, uint64_t bufferOffset)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		std::unique_lock buLock{frameResources.BufferUploadMutex};
		frameResources.BufferToTextureCopies.emplace_back(buffer, texture, bufferOffset, texture->GetNumMips());
	}

	struct TextureBlockInfo
	{
		uint8_t BlockWidth;
		uint8_t BlockHeight;
		uint8_t BytesPerBlock;
	};

	TextureBlockInfo getBlockInfo(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::BC3_SRGB_BLOCK:
			case TextureFormat::BC3_UNORM_BLOCK:
			case TextureFormat::BC5_UNORM_BLOCK:
			case TextureFormat::BC5_SNORM_BLOCK:
				return TextureBlockInfo { 4, 4, 16 };
			default:
				return TextureBlockInfo { 1, 1, 1 };
		}
	}

	uint64_t calculateTextureByteSize(TextureFormat format, uint32_t width, uint32_t height)
	{
		TextureBlockInfo blockInfo = getBlockInfo(format);

		uint32_t blocksX = (width + blockInfo.BlockWidth - 1) / blockInfo.BlockWidth;
		uint32_t blocksY = (height + blockInfo.BlockHeight - 1) / blockInfo.BlockHeight;
		return ((blockInfo.BytesPerBlock + 3) & ~ 3) * blocksX * blocksY;
	}

	void Core::QueueTextureUpload(Texture* texture, void* data, uint64_t dataSize, int numMips)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		std::unique_lock buLock{frameResources.BufferUploadMutex};
		int mipsToUpload = numMips == -1 ? texture->GetNumMips() : numMips;

		if (dataSize >= STAGING_BUFFER_SIZE)
		{
			this->dbgOutRecv->DebugMessage("Queued texture too big to go in staging buffer! THIS IS A STALL");

			VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bci.size = dataSize;
			bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			VmaAllocationCreateInfo vaci{};
			vaci.usage = VMA_MEMORY_USAGE_AUTO;
			vaci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			VkBuffer tempBuffer;
			VmaAllocation tempAlloc;
			VmaAllocationInfo tempAllocInfo{};
			VKCHECK(vmaCreateBuffer(handles.Allocator, &bci, &vaci, &tempBuffer, &tempAlloc, &tempAllocInfo));

			memcpy(tempAllocInfo.pMappedData, data, dataSize);
			std::unique_lock queueLock{queueMutex};

			VKCHECK(vkResetCommandBuffer(frameResources.UploadCommandBuffer, 0));

			// Record upload command buffer
			VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VKCHECK(vkBeginCommandBuffer(frameResources.UploadCommandBuffer, &cbbi));

			texture->Acquire(frameResources.UploadCommandBuffer, ImageLayout::TransferDstOptimal, AccessFlags::TransferWrite, PipelineStageFlags::Transfer);

			uint64_t offset = 0;
			for (int i = 0; i < mipsToUpload; i++)
			{
				int currWidth = mipScale(texture->GetWidth(), i);
				int currHeight = mipScale(texture->GetWidth(), i);
				VkBufferImageCopy vbic{};
				vbic.imageSubresource.layerCount = texture->GetLayers();
				vbic.imageSubresource.mipLevel = i;
				vbic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vbic.imageExtent.width = currWidth;
				vbic.imageExtent.height = currHeight;
				vbic.imageExtent.depth = 1;
				vbic.bufferOffset = offset;

				vkCmdCopyBufferToImage(frameResources.UploadCommandBuffer, tempBuffer,
					texture->GetNativeHandle(),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vbic);

				offset += calculateTextureByteSize(texture->GetFormat(), currWidth, currHeight);
				
			}

			texture->Acquire(frameResources.UploadCommandBuffer, ImageLayout::ReadOnlyOptimal, AccessFlags::MemoryRead, PipelineStageFlags::FragmentShader | PipelineStageFlags::ComputeShader);

			VKCHECK(vkEndCommandBuffer(frameResources.UploadCommandBuffer));

			VkSubmitInfo uploadSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

			uploadSubmitInfo.commandBufferCount = 1;
			uploadSubmitInfo.pCommandBuffers = &frameResources.UploadCommandBuffer;

			VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &uploadSubmitInfo, VK_NULL_HANDLE));
			WaitIdle();

			vmaDestroyBuffer(handles.Allocator, tempBuffer, tempAlloc);

			return;
		}

		uint64_t uploadedOffset = frameResources.StagingOffset;
		uint64_t requiredPadding = 16 - (uploadedOffset % 16);

		if (dataSize + uploadedOffset + requiredPadding >= STAGING_BUFFER_SIZE)
		{
			std::unique_lock queueLock{queueMutex};
			this->dbgOutRecv->DebugMessage("Flushing staged uploads!!! THIS IS A STALL");
			writeFrameUploadCommands(frameIndex);

			VkSubmitInfo uploadSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

			uploadSubmitInfo.commandBufferCount = 1;
			uploadSubmitInfo.pCommandBuffers = &frameResources.UploadCommandBuffer;

			VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &uploadSubmitInfo, VK_NULL_HANDLE));
			WaitIdle();
			uploadedOffset = 0;
			requiredPadding = 0;
		}

		memcpy(frameResources.StagingMapped + uploadedOffset + requiredPadding, data, dataSize);

		frameResources.BufferToTextureCopies.emplace_back(frameResources.StagingBuffer, texture, uploadedOffset + requiredPadding, mipsToUpload);
		frameResources.StagingOffset += dataSize + requiredPadding;
	}

	uint32_t Core::GetFrameIndex() const
	{
		return frameIndex;
	}

	uint32_t Core::GetNextFrameIndex() const
	{
		return getNextFrameIndex(frameIndex);
	}

	uint32_t Core::GetPreviousFrameIndex() const
	{
		return getPreviousFrameIndex(frameIndex);
	}

	uint32_t Core::GetNumFramesInFlight() const
	{
		return NUM_FRAMES_IN_FLIGHT;
	}

	void Core::EndFrame()
	{
		std::unique_lock queueLock{queueMutex};
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		VKCHECK(vkEndCommandBuffer(frameResources.CommandBuffer));

		std::unique_lock uploadLock{frameResources.BufferUploadMutex};
		writeFrameUploadCommands(frameIndex);

		// Submit upload command buffer...
		VkSubmitInfo uploadSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

		uploadSubmitInfo.commandBufferCount = 1;
		uploadSubmitInfo.pCommandBuffers = &frameResources.UploadCommandBuffer;
		uploadSubmitInfo.pSignalSemaphores = &frameResources.UploadSemaphore;
		uploadSubmitInfo.signalSemaphoreCount = 1;

		VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &uploadSubmitInfo, VK_NULL_HANDLE));

		// Then submit main command buffer, waiting on the upload command buffer.
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frameResources.CommandBuffer;
		submitInfo.pWaitSemaphores = &frameResources.UploadSemaphore;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.pSignalSemaphores = &frameResources.Completion;
		submitInfo.signalSemaphoreCount = 1;

		VKCHECK(vkQueueSubmit(handles.Queues.Graphics, 1, &submitInfo, frameResources.Fence));
		inFrame = false;
	}

	void Core::WaitIdle()
	{
		VKCHECK(vkDeviceWaitIdle(handles.Device));
	}

	Core::~Core()
	{
		WaitIdle();

		for (uint32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			frameIndex = i;
			vkFreeCommandBuffers(handles.Device, handles.CommandPool, 1, &perFrameResources[i].CommandBuffer);
			vkFreeCommandBuffers(handles.Device, handles.CommandPool, 1, &perFrameResources[i].UploadCommandBuffer);

			vkDestroySemaphore(handles.Device, perFrameResources[i].Completion, handles.AllocCallbacks);
			vkDestroyFence(handles.Device, perFrameResources[i].Fence, handles.AllocCallbacks);
			perFrameResources[i].StagingBuffer->Unmap();
			delete perFrameResources[i].StagingBuffer;
			vkDestroySemaphore(handles.Device, perFrameResources[i].UploadSemaphore, handles.AllocCallbacks);

			perFrameResources[i].DeletionQueue->Cleanup();
			delete perFrameResources[i].DeletionQueue;
		}

		if (messenger)
		{
			vkDestroyDebugUtilsMessengerEXT(handles.Instance, messenger, handles.AllocCallbacks);
		}

		vmaDestroyAllocator(handles.Allocator);
		vkDestroyCommandPool(handles.Device, handles.CommandPool, handles.AllocCallbacks);
		vkDestroyDevice(handles.Device, handles.AllocCallbacks);
		vkDestroyInstance(handles.Instance, handles.AllocCallbacks);
	}

	const Handles* Core::GetHandles() const
	{
		return &handles;
	}

    IDebugOutputReceiver* Core::GetDebugOutputReceiver()
    {
        return dbgOutRecv;
    }

	void Core::writeFrameUploadCommands(uint32_t index)
	{
		PerFrameResources& frameResources = perFrameResources[index];

		VKCHECK(vkResetCommandBuffer(frameResources.UploadCommandBuffer, 0));

		// Record upload command buffer
		VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VKCHECK(vkBeginCommandBuffer(frameResources.UploadCommandBuffer, &cbbi));

		// Handle pending buffer uploads
		for (BufferUpload& bu : frameResources.BufferUploads)
		{
			frameResources.StagingBuffer->CopyTo(frameResources.UploadCommandBuffer, 
				bu.Buffer, bu.DataSize, bu.StagingOffset, bu.DataOffset);
		}

		for (BufferToTextureCopy& bttc : frameResources.BufferToTextureCopies)
		{
			bttc.Texture->Acquire(frameResources.UploadCommandBuffer, ImageLayout::TransferDstOptimal, AccessFlags::TransferWrite, PipelineStageFlags::Transfer);

			uint64_t offset = 0;
			int w = bttc.Texture->GetWidth();
			int h = bttc.Texture->GetHeight();
			for (int i = 0; i < bttc.numMips; i++)
			{
				VkBufferImageCopy vbic{};
				vbic.imageSubresource.layerCount = bttc.Texture->GetLayers();
				vbic.imageSubresource.mipLevel = i;
				vbic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vbic.imageExtent.width = mipScale(w, i);
				vbic.imageExtent.height = mipScale(h, i);
				vbic.imageExtent.depth = 1;
				vbic.bufferOffset = bttc.BufferOffset + offset;

				vkCmdCopyBufferToImage(frameResources.UploadCommandBuffer, bttc.Buffer->GetNativeHandle(),
					bttc.Texture->GetNativeHandle(),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vbic);

				offset += calculateTextureByteSize(bttc.Texture->GetFormat(), mipScale(w, i), mipScale(h, i));
			}

			bttc.Texture->Acquire(frameResources.UploadCommandBuffer, ImageLayout::ReadOnlyOptimal, AccessFlags::MemoryRead, PipelineStageFlags::FragmentShader | PipelineStageFlags::ComputeShader);
		}

		// Reset the queue
		frameResources.BufferUploads.clear();
		frameResources.BufferToTextureCopies.clear();
		frameResources.StagingOffset = 0;

		VKCHECK(vkEndCommandBuffer(frameResources.UploadCommandBuffer));
	}

    DeletionQueue* Core::getCurrentDq()
    {
        return perFrameResources[frameIndex].DeletionQueue;
    }
}
