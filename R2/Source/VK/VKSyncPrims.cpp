#include <R2/VKSyncPrims.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDeletionQueue.hpp>
#include <volk.h>

namespace R2::VK
{
    Fence::Fence(const Handles* handles, FenceFlags flags)
        : handles(handles)
    {
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

        if ((flags & FenceFlags::CreateSignaled) == FenceFlags::CreateSignaled)
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VKCHECK(vkCreateFence(handles->Device, &fci, handles->AllocCallbacks, &fence));
    }

    void Fence::WaitFor()
    {
        VKCHECK(vkWaitForFences(handles->Device, 1, &fence, VK_TRUE, UINT64_MAX));
    }

    void Fence::Reset()
    {
        VKCHECK(vkResetFences(handles->Device, 1, &fence));
    }

    VkFence Fence::GetNativeHandle()
    {
        return fence;
    }

    Fence::~Fence()
    {
        vkDestroyFence(handles->Device, fence, handles->AllocCallbacks);
    }

    Event::Event(Core *core)
        : core(core)
    {
        const Handles* handles = core->GetHandles();
        VkEventCreateInfo eci{ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
        VKCHECK(vkCreateEvent(handles->Device, &eci, handles->AllocCallbacks, &event));
    }

    bool Event::IsSet()
    {
        const Handles* handles = core->GetHandles();
        return vkGetEventStatus(handles->Device, event) == VK_EVENT_SET;
    }

    void Event::Set()
    {
        const Handles* handles = core->GetHandles();
        VKCHECK(vkSetEvent(handles->Device, event));
    }

    void Event::Reset()
    {
        const Handles* handles = core->GetHandles();
        VKCHECK(vkResetEvent(handles->Device, event));
    }

    VkEvent Event::GetNativeHandle()
    {
        return event;
    }

    Event::~Event()
    {
        core->getCurrentDq()->QueueObjectDeletion(event, VK_OBJECT_TYPE_EVENT);
    }
}