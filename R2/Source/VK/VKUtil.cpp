#include <R2/VKUtil.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKEnums.hpp>
#include <volk.h>

namespace R2::VK
{
    const Handles* Utils::handles;
    VkCommandBuffer Utils::immediateCommandBuffer;

    void Utils::SetupImmediateCommandBuffer(const Handles* handles)
    {
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandBufferCount = 1;
        cbai.commandPool = handles->CommandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VKCHECK(vkAllocateCommandBuffers(handles->Device, &cbai, &immediateCommandBuffer));
        Utils::handles = handles;
    }

    VkCommandBuffer Utils::AcquireImmediateCommandBuffer()
    {
        VKCHECK(vkResetCommandBuffer(immediateCommandBuffer, 0));
        VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VKCHECK(vkBeginCommandBuffer(immediateCommandBuffer, &cbbi));

        return immediateCommandBuffer;
    }

    void Utils::ExecuteImmediateCommandBuffer()
    {
        VKCHECK(vkEndCommandBuffer(immediateCommandBuffer));

        VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &immediateCommandBuffer;

        VKCHECK(vkQueueSubmit(handles->Queues.Graphics, 1, &submitInfo, VK_NULL_HANDLE));
        vkDeviceWaitIdle(handles->Device);
    }

    bool hasAF(AccessFlags access, AccessFlags test)
    {
        return ((uint64_t)access & (uint64_t)test) != 0;
    }

    PipelineStageFlags getPipelineStage(AccessFlags access)
    {
        PipelineStageFlags pFlags = PipelineStageFlags::None;

        if (hasAF(access, AccessFlags::ColorAttachmentReadWrite))
        {
            pFlags |= PipelineStageFlags::ColorAttachmentOutput;
        }

        if (hasAF(access, AccessFlags::DepthStencilAttachmentReadWrite))
        {
            pFlags |= PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests;
        }

        if (hasAF(access, AccessFlags::ShaderRead | AccessFlags::ShaderWrite))
        {
            pFlags |= PipelineStageFlags::VertexShader | PipelineStageFlags::FragmentShader | PipelineStageFlags::ComputeShader;
        }

        if (hasAF(access, AccessFlags::IndexRead))
        {
            pFlags |= PipelineStageFlags::IndexInput;
        }

        if (hasAF(access, AccessFlags::HostRead | AccessFlags::HostWrite))
        {
            pFlags |= PipelineStageFlags::Host;
        }

        if (hasAF(access, AccessFlags::TransferRead | AccessFlags::TransferWrite))
        {
            pFlags |= PipelineStageFlags::Transfer;
        }

        if (pFlags == PipelineStageFlags::None)
        {
            pFlags = PipelineStageFlags::AllGraphics;
        }

        return pFlags;
    }
}