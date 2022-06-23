#include <R2/VKUtil.hpp>
#include <R2/VKCore.hpp>
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
}