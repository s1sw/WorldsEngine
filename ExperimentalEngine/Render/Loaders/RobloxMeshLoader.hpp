#pragma once
#include "Engine.hpp"
#include "Render.hpp"

namespace worlds {
    void loadRobloxMesh(AssetID id, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd);
}