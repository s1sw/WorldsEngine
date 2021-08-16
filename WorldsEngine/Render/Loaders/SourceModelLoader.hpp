#pragma once
#include "../../Core/Engine.hpp"
#include "../../Core/AssetDB.hpp"
#include <vector>
#include "../RenderInternal.hpp"

namespace worlds {
    void loadSourceModel(AssetID mdlId, AssetID vtxId, AssetID vvdId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd);
    void setupSourceMaterials(AssetID mdlId, WorldObject& wo);
}
