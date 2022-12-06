#pragma once
#include <mutex>
#include <entt/entity/lw_fwd.hpp>

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
        static void Shutdown();
        static R2::VK::Buffer* GetBuffer();
        static bool IsMaterialLoaded(AssetID id);
        static unsigned int LoadOrGetMaterial(AssetID id);
        static unsigned int GetMaterial(AssetID id);
        static bool IsMaterialAlphaTest(AssetID id);
        static void Unload(AssetID id);
        static void UnloadUnusedMaterials(entt::registry& reg);
        static void ShowDebugMenu();
    };
}