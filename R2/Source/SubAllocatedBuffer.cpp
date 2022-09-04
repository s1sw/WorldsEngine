#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKBuffer.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

namespace R2
{
    SubAllocatedBuffer::SubAllocatedBuffer(VK::Core* core, const VK::BufferCreateInfo& ci)
    {
        buf = core->CreateBuffer(ci);
        VmaVirtualBlockCreateInfo vbci{};
        vbci.size = ci.Size;

        VKCHECK(vmaCreateVirtualBlock(&vbci, &virtualBlock));
    }

    SubAllocatedBuffer::~SubAllocatedBuffer()
    {
        delete buf;
        vmaDestroyVirtualBlock(virtualBlock);
    }

    VK::Buffer* SubAllocatedBuffer::GetBuffer()
    {
        return buf;
    }

    uint64_t SubAllocatedBuffer::Allocate(uint64_t amount, SubAllocationHandle& allocation)
    {
        std::lock_guard lg{mutex};
        VmaVirtualAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.size = amount;

        size_t offset;
        VKCHECK(vmaVirtualAllocate(virtualBlock, &allocCreateInfo, (VmaVirtualAllocation*)&allocation, &offset));

        return offset;
    }

    void SubAllocatedBuffer::Free(SubAllocationHandle subAllocHandle)
    {
        std::lock_guard lg{mutex};
        vmaVirtualFree(virtualBlock, (VmaVirtualAllocation)subAllocHandle);
    }
}