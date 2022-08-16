#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/WMDLLoader.hpp>
#include <Render/RenderInternal.hpp>

namespace worlds
{
    RenderMeshManager::RenderMeshManager(R2::VK::Core* core) : core(core)
    {
        R2::VK::BufferCreateInfo createInfo{};
        createInfo.Size = 8 * 1000 * 1000; // 8MB
        createInfo.Usage = R2::VK::BufferUsage::Vertex | R2::VK::BufferUsage::Storage;
        vertexBuffer = new R2::SubAllocatedBuffer(core, createInfo);

        createInfo.Size = 8 * 1000 * 1000; // 8MB
        createInfo.Usage = R2::VK::BufferUsage::Index | R2::VK::BufferUsage::Storage;
        indexBuffer = new R2::SubAllocatedBuffer(core, createInfo);
    }

    RenderMeshManager::~RenderMeshManager()
    {
        for (auto p : meshes)
        {
            vertexBuffer->Free(p.second.vertexAllocationHandle);
            indexBuffer->Free(p.second.indexAllocationHandle);
        }

        delete indexBuffer;
        delete vertexBuffer;
    }

    R2::VK::Buffer* RenderMeshManager::getVertexBuffer()
    {
        return vertexBuffer->GetBuffer();
    }

    R2::VK::Buffer* RenderMeshManager::getIndexBuffer()
    {
        return indexBuffer->GetBuffer();
    }

    RenderMeshInfo& RenderMeshManager::loadOrGet(AssetID asset)
    {
        if (meshes.contains(asset))
        {
            return meshes.at(asset);
        }

        LoadedMeshData lmd{};
        loadWorldsModel(asset, lmd);

        size_t indicesSize = lmd.indexType == IndexType::Uint16 ? lmd.indices16.size() * sizeof(uint16_t)
                                                                : lmd.indices32.size() * sizeof(uint32_t);

        RenderMeshInfo meshInfo{};
        meshInfo.indexOffset = indexBuffer->Allocate(indicesSize, meshInfo.indexAllocationHandle);
        meshInfo.vertsOffset =
            vertexBuffer->Allocate(lmd.vertices.size() * sizeof(Vertex), meshInfo.vertexAllocationHandle);
        meshInfo.indexType = lmd.indexType;

        meshInfo.numSubmeshes = lmd.submeshes.size();
        for (int i = 0; i < lmd.submeshes.size(); i++)
        {
            meshInfo.submeshInfo[i].indexCount = lmd.submeshes[i].indexCount;
            meshInfo.submeshInfo[i].indexOffset = lmd.submeshes[i].indexOffset;
            meshInfo.submeshInfo[i].materialIndex = lmd.submeshes[i].materialIndex;
        }

        if (lmd.indexType == IndexType::Uint16)
        {
            core->QueueBufferUpload(indexBuffer->GetBuffer(), lmd.indices16.data(), indicesSize, meshInfo.indexOffset);
        }
        else
        {
            core->QueueBufferUpload(indexBuffer->GetBuffer(), lmd.indices32.data(), indicesSize, meshInfo.indexOffset);
        }

        core->QueueBufferUpload(
            vertexBuffer->GetBuffer(), 
            lmd.vertices.data(), lmd.vertices.size() * sizeof(Vertex),
            meshInfo.vertsOffset
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

        meshes.insert({asset, std::move(meshInfo)});

        return meshes.at(asset);
    }
}