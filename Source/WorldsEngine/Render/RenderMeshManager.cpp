#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/WMDLLoader.hpp>
#include <Render/RenderInternal.hpp>
#include <Tracy.hpp>
#include <mutex>

namespace worlds
{
    AssetID missingModel;
    std::mutex modelMutex;

    RenderMeshManager::RenderMeshManager(R2::VK::Core* core) : core(core)
    {
        R2::VK::BufferCreateInfo createInfo{};
        createInfo.Size = 8 * 1000 * 1000 * sizeof(Vertex); // 8 million vertices
        createInfo.Usage = R2::VK::BufferUsage::Vertex | R2::VK::BufferUsage::Storage;
        vertexBuffer = new R2::SubAllocatedBuffer(core, createInfo);

        createInfo.Size = 9 * 1000 * 1000 * sizeof(uint32_t); // 3 million triangles
        createInfo.Usage = R2::VK::BufferUsage::Index | R2::VK::BufferUsage::Storage;
        indexBuffer = new R2::SubAllocatedBuffer(core, createInfo);

        createInfo.Size = 1 * 1000 * 1000 * sizeof(VertexSkinInfo); // 1 million skinned vertices
        createInfo.Usage = R2::VK::BufferUsage::Storage;
        skinInfoBuffer = new R2::SubAllocatedBuffer(core, createInfo);

        missingModel = AssetDB::pathToId("Models/missing.wmdl");
        loadOrGet(missingModel);

        skinnedVertsOffset = vertexBuffer->Allocate(1 * 1000 * 1000 * sizeof(Vertex), skinnedVertsAllocation);
        AssetDB::registerAssetChangeCallback(
            [&](AssetID id) {
                if (meshes.contains(id))
                {
                    RenderMeshInfo& rmi = meshes[id];

                    if (rmi.vertexAllocationHandle != meshes[missingModel].vertexAllocationHandle)
                    {
                        vertexBuffer->Free(rmi.vertexAllocationHandle);
                        indexBuffer->Free(rmi.indexAllocationHandle);

                        if (rmi.skinInfoAllocationHandle != nullptr)
                            skinInfoBuffer->Free(rmi.skinInfoAllocationHandle);
                    }

                    loadToRMI(id, rmi);
                }
            }
        );
    }

    RenderMeshManager::~RenderMeshManager()
    {
        for (auto& p : meshes)
        {
            vertexBuffer->Free(p.second.vertexAllocationHandle);
            indexBuffer->Free(p.second.indexAllocationHandle);

            if (p.second.skinInfoAllocationHandle != nullptr)
                skinInfoBuffer->Free(p.second.skinInfoAllocationHandle);
        }

        vertexBuffer->Free(skinnedVertsAllocation);

        delete indexBuffer;
        delete vertexBuffer;
        delete skinInfoBuffer;
    }

    R2::VK::Buffer* RenderMeshManager::getVertexBuffer()
    {
        return vertexBuffer->GetBuffer();
    }

    R2::VK::Buffer* RenderMeshManager::getIndexBuffer()
    {
        return indexBuffer->GetBuffer();
    }

    R2::VK::Buffer* RenderMeshManager::getSkinInfoBuffer()
    {
        return skinInfoBuffer->GetBuffer();
    }

    RenderMeshInfo& RenderMeshManager::loadOrGet(AssetID asset)
    {
        ZoneScoped;
        std::lock_guard lg{modelMutex};
        if (meshes.contains(asset))
        {
            return meshes.at(asset);
        }

        RenderMeshInfo meshInfo{};

        loadToRMI(asset, meshInfo);

        auto it = meshes.insert({asset, std::move(meshInfo)}).first;

        return meshes.at(asset);
    }

    bool RenderMeshManager::get(AssetID id, RenderMeshInfo** rmi)
    {
        auto it = meshes.find(id);
        if (it == meshes.end()) return false;
        *rmi = &it->second;
        return true;
    }

    uint64_t RenderMeshManager::getSkinnedVertsOffset() const
    {
        return skinnedVertsOffset;
    }

    void RenderMeshManager::loadToRMI(AssetID asset, RenderMeshInfo& meshInfo)
    {
        LoadedMeshData lmd{};
        if (!loadWorldsModel(asset, lmd))
        {
            loadWorldsModel(AssetDB::pathToId("Models/missing.wmdl"), lmd);
        }

        if (lmd.vertices.size() == 0)
        {
            meshInfo = meshes.at(missingModel);
        }

        // We don't support 16 bit indices anymore so we can bind the index buffer just once.
        if (lmd.indexType == IndexType::Uint16)
        {
            lmd.indices32.resize(lmd.indices16.size());
            for (size_t i = 0; i < lmd.indices16.size(); i++)
            {
                lmd.indices32[i] = (uint32_t)lmd.indices16[i];
            }
        }

        size_t indicesSize = lmd.indexType == IndexType::Uint16 ? lmd.indices16.size() * sizeof(uint32_t)
                                                                : lmd.indices32.size() * sizeof(uint32_t);

        meshInfo.indexOffset = indexBuffer->Allocate(indicesSize, meshInfo.indexAllocationHandle);
        meshInfo.vertsOffset =
            vertexBuffer->Allocate(lmd.vertices.size() * sizeof(Vertex), meshInfo.vertexAllocationHandle);

        meshInfo.numSubmeshes = lmd.submeshes.size();
        for (int i = 0; i < lmd.submeshes.size(); i++)
        {
            meshInfo.submeshInfo[i].indexCount = lmd.submeshes[i].indexCount;
            meshInfo.submeshInfo[i].indexOffset = lmd.submeshes[i].indexOffset;
            meshInfo.submeshInfo[i].materialIndex = lmd.submeshes[i].materialIndex;
        }

        meshInfo.numVertices = lmd.vertices.size();

        core->QueueBufferUpload(indexBuffer->GetBuffer(), lmd.indices32.data(), indicesSize, meshInfo.indexOffset);

        core->QueueBufferUpload(
            vertexBuffer->GetBuffer(), lmd.vertices.data(), lmd.vertices.size() * sizeof(Vertex), meshInfo.vertsOffset
        );

        meshInfo.aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
        meshInfo.aabbMin = glm::vec3(std::numeric_limits<float>::max());
        meshInfo.boundingSphereRadius = 0.0f;

        for (const Vertex& vtx : lmd.vertices)
        {
            meshInfo.boundingSphereRadius = glm::max(glm::length(vtx.position), meshInfo.boundingSphereRadius);
            meshInfo.aabbMax = glm::max(meshInfo.aabbMax, vtx.position);
            meshInfo.aabbMin = glm::min(meshInfo.aabbMin, vtx.position);
        }

        if (lmd.isSkinned)
        {
            meshInfo.skinInfoOffset = skinInfoBuffer->Allocate(
                lmd.skinningInfos.size() * sizeof(VertexSkinInfo), meshInfo.skinInfoAllocationHandle
            );

            core->QueueBufferUpload(
                skinInfoBuffer->GetBuffer(),
                lmd.skinningInfos.data(),
                lmd.skinningInfos.size() * sizeof(VertexSkinInfo),
                meshInfo.skinInfoOffset
            );
        }
    }

}