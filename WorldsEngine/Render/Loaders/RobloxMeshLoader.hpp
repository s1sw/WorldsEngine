#pragma once
#include "../../Core/Engine.hpp"
#include "../RenderInternal.hpp"

namespace worlds {
    void loadRobloxMesh(AssetID id, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd);
}
