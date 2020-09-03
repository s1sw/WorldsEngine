#pragma once
#include "Engine.hpp"
#include "AssetDB.hpp"
#include <vector>

namespace worlds {
    void loadSourceModel(AssetID mdlId, AssetID vtxId, AssetID vvdId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
}