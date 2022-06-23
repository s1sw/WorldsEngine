#pragma once
#include <vector>

typedef struct HWND__* HWND;

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkSwapchainKHR)
VK_DEFINE_HANDLE(VkSurfaceKHR)
VK_DEFINE_HANDLE(VkFence)
VK_DEFINE_HANDLE(VkSemaphore)
VK_DEFINE_HANDLE(VkImage)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;
    class Texture;
    class Fence;
    class Core;

    struct SwapchainCreateInfo
    {
        VkSurfaceKHR surface;
    };

    class Swapchain
    {
    public:
        Swapchain(Core* renderer, const SwapchainCreateInfo& createInfo);
        ~Swapchain();

        void SetVsync(bool vsync);
        void Present();
        void Resize();
        void Resize(int width, int height);
        
        void GetSize(int& width, int& height);
        Texture* Acquire(Fence* fence);
    private:
        void recreate();
        void recreate(int width, int height);
        void destroySwapchain();
        const Handles* handles;
        Core* renderer;
        VkSwapchainKHR swapchain;
        VkSurfaceKHR surface;
        std::vector<VkImage> images;
        std::vector<Texture*> imageTextures;
        uint32_t acquiredImageIndex;
        bool vsyncEnabled;
        int width, height;
    };
}