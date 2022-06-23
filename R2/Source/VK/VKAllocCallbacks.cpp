#include <R2/VKCore.hpp>
#include <volk.h>
#include <stdlib.h>

namespace R2::VK
{
	void* vulkanAlloc(void* userData, size_t size, size_t alignment, VkSystemAllocationScope scope)
	{
		return _aligned_malloc(size, alignment);
	}

	void vulkanFree(void* userData, void* memory)
	{
		return _aligned_free(memory);
	}

	void* vulkanRealloc(void* userData, void* original, size_t size, size_t alignment, VkSystemAllocationScope scope)
	{
		return _aligned_realloc(original, size, alignment);
	}

	void Core::setAllocCallbacks()
	{
		VkAllocationCallbacks* callbacks = new VkAllocationCallbacks;
		callbacks->pfnAllocation = vulkanAlloc;
		callbacks->pfnFree = vulkanFree;
		callbacks->pfnReallocation = vulkanRealloc;
		handles.AllocCallbacks = callbacks;
	}
}