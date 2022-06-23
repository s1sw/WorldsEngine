#pragma once

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkFence)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;

    enum class FenceFlags
    {
        None = 0,
        CreateSignaled = 1
    };

    inline FenceFlags operator&(const FenceFlags& a, const FenceFlags& b)
    {
        return static_cast<FenceFlags>(static_cast<int>(a) & static_cast<int>(b));
    }

    class Fence
    {
    public:
        Fence(const Handles* handles, FenceFlags flags);
        void WaitFor();
        void Reset();
        VkFence GetNativeHandle();
        ~Fence();
    private:
        const Handles* handles;
        VkFence fence;
    };
}