#pragma once
#include <mutex>

namespace R2
{
    class SubAllocatedBuffer;

    namespace VK
    {
        class Buffer;
    }
}

namespace worlds
{
    class VKRenderer;
    typedef uint32_t AssetID;

    class RenderMaterialManager
    {
        static std::mutex mutex;
        static R2::SubAllocatedBuffer* materialBuffer;
        static VKRenderer* renderer;
    public:
        static bool IsInitialized();
        static void Initialize(VKRenderer* renderer);
        static R2::VK::Buffer* GetBuffer();
        static bool IsMaterialLoaded(AssetID id);
        static size_t LoadOrGetMaterial(AssetID id);
    };
}