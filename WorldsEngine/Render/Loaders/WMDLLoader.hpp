#pragma once
#include "../../Core/Engine.hpp"
#include "../RenderInternal.hpp"

namespace worlds {
    void loadWorldsModel(AssetID wmdlId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd);
}
