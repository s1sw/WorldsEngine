#pragma once
#include <mutex>
#include <vector>
#include <entt/entity/lw_fwd.hpp>

namespace R2
{
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
    VK_DEFINE_HANDLE(SubAllocationHandle);
#undef VK_DEFINE_HANDLE
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

    struct MaterialInfo
    {
        uint32_t offset;
        R2::SubAllocationHandle handle;
        std::vector<AssetID> referencedTextures;
        bool alphaTest;
        AssetID fragmentShader;
        AssetID vertexShader;
    };

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
        static const MaterialInfo& GetMaterialInfo(AssetID id);
        static void Unload(AssetID id);
        static void UnloadUnusedMaterials(entt::registry& reg);
        static void ShowDebugMenu();
    };
}