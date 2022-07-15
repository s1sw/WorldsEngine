#pragma once
#include <Core/Engine.hpp>
#include <Render/RenderInternal.hpp>
#include <vector>

namespace worlds
{
    struct LoadedMeshBone
    {
        glm::mat4 inverseBindPose;
        glm::mat4 transform;
        uint32_t parentIdx;
        std::string name;
    };

    struct LoadedSubmesh
    {
        uint32_t indexCount;
        uint32_t indexOffset;
        uint32_t materialIndex;
    };

    struct LoadedMeshData
    {
        bool isSkinned;
        std::vector<LoadedMeshBone> bones;
        std::vector<LoadedSubmesh> submeshes;
        IndexType indexType;
        std::vector<uint16_t> indices16;
        std::vector<uint32_t> indices32;
        std::vector<Vertex> vertices;
        std::vector<VertSkinningInfo> skinningInfos;
    };

    void loadWorldsModel(AssetID wmdlId, LoadedMeshData& lmd);
}
