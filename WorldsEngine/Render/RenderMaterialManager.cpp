#include "Render/RenderMaterialManager.hpp"
#include <Core/AssetDB.hpp>
#include <Core/MaterialManager.hpp>
#include <entt/entity/registry.hpp>
#include <ImGui/imgui.h>
#include <R2/VK.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <Render/RenderInternal.hpp>
#include <Util/JsonUtil.hpp>

using namespace R2;

namespace worlds
{
    struct MaterialAllocInfo
    {
        uint32_t offset;
        SubAllocationHandle handle;
        AssetID albedoID = ~0u;
        AssetID normalID = ~0u;
        AssetID mraID = ~0u;
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
        uint32_t cutoffFlags;

        float getCutoff()
        {
            return (cutoffFlags & 0xFF) / 255.0f;
        }

        void setCutoff(float cutoff)
        {
            uint32_t flags = cutoffFlags >> 8;
            cutoff = glm::clamp(cutoff, 0.0f, 1.0f);
            cutoffFlags = (flags << 8) | (uint32_t)(cutoff * 255);
        }

        void setFlags(uint32_t flags)
        {
            uint32_t cutoff = (cutoffFlags & 0xFF);
            uint32_t flag = (uint32_t)(flags) << 8;
            cutoffFlags = cutoff | flag;
        }

        uint32_t getFlags()
        {
            return (cutoffFlags & (0x7FFFFF80)) >> 8;
        }
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

    void RenderMaterialManager::Shutdown()
    {
        for (auto& p : allocedMaterials)
        {
            materialBuffer->Free(p.second.handle);
        }

        delete materialBuffer;
    }

    R2::VK::Buffer* RenderMaterialManager::GetBuffer()
    {
        return materialBuffer->GetBuffer();
    }

    bool RenderMaterialManager::IsMaterialLoaded(AssetID id)
    {
        return allocedMaterials.contains(id);
    }

    uint32_t RenderMaterialManager::LoadOrGetMaterial(AssetID id)
    {
        MaterialAllocInfo mai{};
        {
            std::unique_lock lock{mutex};

            if (allocedMaterials.contains(id))
            {
                return allocedMaterials.at(id).offset;
            }

            mai.offset = (uint32_t)materialBuffer->Allocate(sizeof(StandardPBRMaterial), mai.handle);
            allocedMaterials.insert({id, mai});
        }

        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* tm = renderer->getTextureManager();

        nlohmann::json& j = MaterialManager::loadOrGet(id);

        StandardPBRMaterial material{};
        mai.albedoID = AssetDB::pathToId(j.value("albedoPath", "Textures/missing.wtex"));
        material.albedoTexture = tm->loadAndGetAsync(mai.albedoID);
        material.normalTexture = ~0u;
        material.mraTexture = ~0u;

        if (j.contains("normalMapPath"))
        {
            mai.normalID = AssetDB::pathToId(j["normalMapPath"].get<std::string>());
            material.normalTexture = tm->loadAndGetAsync(mai.normalID);
        }

        if (j.contains("pbrMapPath"))
        {
            mai.mraID = AssetDB::pathToId(j["pbrMapPath"].get<std::string>());
            material.mraTexture = tm->loadAndGetAsync(mai.mraID);
        }

        material.defaultMetallic = j.value("metallic", 0.0f);
        material.defaultRoughness = j.value("roughness", 0.5f);
        if (j.contains("emissiveColor"))
            material.emissiveColor = j["emissiveColor"].get<glm::vec3>();
        else
            material.emissiveColor = glm::vec3(0.0f);

        if (j.value("multiplyEmissiveByAlbedo", false))
        {
            material.setFlags(1);
        }
        material.setCutoff(j.value("alphaCutoff", 0.0));

        renderer->getCore()->QueueBufferUpload(materialBuffer->GetBuffer(), &material, sizeof(material), mai.offset);
        allocedMaterials[id] = mai;

        return mai.offset;
    }

    uint32_t RenderMaterialManager::GetMaterial(AssetID id)
    {
        return allocedMaterials.at(id).offset;
    }

    void RenderMaterialManager::Unload(AssetID id)
    {
        MaterialAllocInfo mai = allocedMaterials.at(id);
        materialBuffer->Free(mai.handle);
        allocedMaterials.erase(id);

        VKTextureManager* tm = renderer->getTextureManager();
        if (mai.albedoID != ~0u) tm->release(mai.albedoID);
        if (mai.normalID != ~0u) tm->release(mai.normalID);
        if (mai.mraID != ~0u) tm->release(mai.mraID);
    }

    robin_hood::unordered_map<AssetID, int> materialRefCounts;
    void RenderMaterialManager::UnloadUnusedMaterials(entt::registry& reg)
    {
        materialRefCounts.clear();
        materialRefCounts.reserve(allocedMaterials.size());

        for (auto& p : allocedMaterials)
        {
            materialRefCounts.insert({p.first, 0});
        }

        reg.view<WorldObject>().each([&](WorldObject& wo) {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++)
            {
                if (!wo.presentMaterials[i]) continue;

                materialRefCounts[wo.materials[i]]++;
            }
        });

        reg.view<SkinnedWorldObject>().each([&](SkinnedWorldObject& wo) {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++)
            {
                if (!wo.presentMaterials[i]) continue;

                materialRefCounts[wo.materials[i]]++;
            }
        });

        for (auto& p : materialRefCounts)
        {
            if (p.second == 0)
                Unload(p.first);
        }
    }

    void RenderMaterialManager::ShowDebugMenu()
    {
        if (ImGui::Begin("Material Manager"))
        {
            ImGui::Text("%zu materials loaded", allocedMaterials.size());
        }

        ImGui::End();
    }
}