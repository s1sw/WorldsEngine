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

size_t operator""_KB(size_t sz)
{
	return sz * 1000;
}

size_t operator""_MB(size_t sz)
{
	return sz * 1000 * 1000;
}

namespace R2::VK
{
	const uint32_t NUM_FRAMES_IN_FLIGHT = 2;
	const size_t STAGING_BUFFER_SIZE = 16_MB;

	Core::Core(IDebugOutputReceiver* dbgOutRecv)
		: inFrame(false)
		, frameIndex(0)
	{
		setAllocCallbacks();
		createInstance(true);
		findQueueFamilies();
		createDevice();
		createCommandPool();
		createAllocator();
		createDescriptorPool();

		Utils::SetupImmediateCommandBuffer(GetHandles());

		for (uint32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			perFrameResources[i] = PerFrameResources{};
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

	GraphicsDeviceInfo Core::GetDeviceInfo()
	{
		GraphicsDeviceInfo deviceInfo{};
		VkPhysicalDeviceProperties deviceProps{};
		vkGetPhysicalDeviceProperties(handles.PhysicalDevice, &deviceProps);

		strncpy(deviceInfo.Name, deviceProps.deviceName, 256);

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
		return new DescriptorSet(GetHandles(), ds);
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
		return new DescriptorSet(GetHandles(), ds);
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

		// Now we know that the command buffer has finished executing, so we can
		// go through the deletion queue and clean up
		frameResources.DeletionQueue->Cleanup();

		// Prepare the command buffer for recording
		VKCHECK(vkResetCommandBuffer(frameResources.CommandBuffer, 0));

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

	void Core::QueueBufferUpload(Buffer* buffer, void* data, uint64_t dataSize, uint64_t dataOffset)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		memcpy(frameResources.StagingMapped + frameResources.StagingOffset, data, dataSize);

		frameResources.BufferUploads.emplace_back(buffer, frameResources.StagingOffset, dataSize, dataOffset);
		frameResources.StagingOffset += dataSize;
	}

	void Core::QueueBufferToTextureCopy(Buffer* buffer, Texture* texture, uint64_t bufferOffset)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		frameResources.BufferToTextureCopies.emplace_back(buffer, texture, bufferOffset);
	}

	void Core::QueueTextureUpload(Texture* texture, void* data, uint64_t dataSize)
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];

		uint64_t uploadedOffset = frameResources.StagingOffset;
		if (dataSize + uploadedOffset >= STAGING_BUFFER_SIZE) abort();

		memcpy(frameResources.StagingMapped + uploadedOffset, data, dataSize);

		frameResources.BufferToTextureCopies.emplace_back(frameResources.StagingBuffer, texture, uploadedOffset);

		frameResources.StagingOffset += dataSize;
	}

	uint32_t Core::GetFrameIndex() const
	{
		return frameIndex;
	}

	void Core::EndFrame()
	{
		PerFrameResources& frameResources = perFrameResources[frameIndex];
		VKCHECK(vkEndCommandBuffer(frameResources.CommandBuffer));

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

		frameResources.BufferUploads.clear();

		for (BufferToTextureCopy& bttc : frameResources.BufferToTextureCopies)
		{
			VkBufferImageCopy vbic{};
			vbic.imageSubresource.layerCount = 1;
			vbic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vbic.imageExtent.width = bttc.Texture->GetWidth();
			vbic.imageExtent.height = bttc.Texture->GetHeight();
			vbic.imageExtent.depth = 1;
			vbic.bufferOffset = bttc.BufferOffset;

			bttc.Texture->WriteLayoutTransition(frameResources.UploadCommandBuffer, ImageLayout::TransferDstOptimal);

			vkCmdCopyBufferToImage(frameResources.UploadCommandBuffer, bttc.Buffer->GetNativeHandle(),
				bttc.Texture->GetNativeHandle(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vbic);

			bttc.Texture->WriteLayoutTransition(frameResources.UploadCommandBuffer,
				ImageLayout::TransferDstOptimal, ImageLayout::ReadOnlyOptimal);
		}

		frameResources.BufferToTextureCopies.clear();

		frameResources.StagingOffset = 0;

		VKCHECK(vkEndCommandBuffer(frameResources.UploadCommandBuffer));

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
			vkFreeCommandBuffers(handles.Device, handles.CommandPool, 1, &perFrameResources[i].CommandBuffer);
			vkFreeCommandBuffers(handles.Device, handles.CommandPool, 1, &perFrameResources[i].UploadCommandBuffer);

			vkDestroySemaphore(handles.Device, perFrameResources[i].Completion, handles.AllocCallbacks);
			vkDestroyFence(handles.Device, perFrameResources[i].Fence, handles.AllocCallbacks);
			perFrameResources[i].DeletionQueue->Cleanup();
			delete perFrameResources[i].DeletionQueue;

			vkDestroySemaphore(handles.Device, perFrameResources[i].UploadSemaphore, handles.AllocCallbacks);
			perFrameResources[i].StagingBuffer->Unmap();
			delete perFrameResources[i].StagingBuffer;
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
}
