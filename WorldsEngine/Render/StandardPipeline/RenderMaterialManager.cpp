#include "RenderMaterialManager.hpp"
#include <Core/AssetDB.hpp>
#include <Core/MaterialManager.hpp>
#include <R2/VK.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <Render/RenderInternal.hpp>
#include <Util/JsonUtil.hpp>

using namespace R2;

namespace worlds
{
    struct MaterialAllocInfo
    {
        size_t offset;
        SubAllocationHandle handle;
    };

    robin_hood::unordered_map<AssetID, MaterialAllocInfo> allocedMaterials;

    std::mutex RenderMaterialManager::mutex{};
    R2::SubAllocatedBuffer* RenderMaterialManager::materialBuffer = nullptr;
    VKRenderer* RenderMaterialManager::renderer = nullptr;

    struct StandardPBRMaterial
    {
        uint32_t albedoTexture;
        uint32_t normalTexture;
        uint32_t mraTexture;
        float defaultRoughness;
        float defaultMetallic;
        glm::vec3 emissiveColor;
    };

    bool RenderMaterialManager::IsInitialized()
    {
        return renderer != nullptr;
    }

    void RenderMaterialManager::Initialize(VKRenderer* renderer)
    {
        RenderMaterialManager::renderer = renderer;
        VK::BufferCreateInfo materialBci{VK::BufferUsage::Storage, sizeof(uint32_t) * 8 * 128, true};
        materialBuffer = new SubAllocatedBuffer(renderer->getCore(), materialBci);
        materialBuffer->GetBuffer()->SetDebugName("Material Buffer");
    }

    R2::VK::Buffer* RenderMaterialManager::GetBuffer()
    {
        return materialBuffer->GetBuffer();
    }

    bool RenderMaterialManager::IsMaterialLoaded(AssetID id)
    {
        return allocedMaterials.contains(id);
    }

    size_t RenderMaterialManager::LoadOrGetMaterial(AssetID id)
    {
        MaterialAllocInfo mai{};
        {
            std::unique_lock lock{mutex};

            if (allocedMaterials.contains(id))
            {
                return allocedMaterials[id].offset;
            }

            mai.offset = materialBuffer->Allocate(sizeof(StandardPBRMaterial), mai.handle);
            allocedMaterials.insert({id, mai});
        }

        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* tm = renderer->getTextureManager();

        nlohmann::json& j = MaterialManager::loadOrGet(id);

        StandardPBRMaterial material{};
        material.albedoTexture = tm->loadOrGet(AssetDB::pathToId(j.value("albedoPath", "Textures/missing.wtex")));
        material.normalTexture = ~0u;
        material.mraTexture = ~0u;

        if (j.contains("normalMapPath"))
        {
            material.normalTexture = tm->loadOrGet(AssetDB::pathToId(j["normalMapPath"]));
        }

        if (j.contains("pbrMapPath"))
        {
            material.mraTexture = tm->loadOrGet(AssetDB::pathToId(j["pbrMapPath"]));
        }

        material.defaultMetallic = j.value("metallic", 0.0f);
        material.defaultRoughness = j.value("roughness", 0.5f);
        if (j.contains("emissiveColor"))
            material.emissiveColor = j["emissiveColor"].get<glm::vec3>();
        else
            material.emissiveColor = glm::vec3(0.0f);

        renderer->getCore()->QueueBufferUpload(materialBuffer->GetBuffer(), &material, sizeof(material), mai.offset);

        return mai.offset;
    }
}