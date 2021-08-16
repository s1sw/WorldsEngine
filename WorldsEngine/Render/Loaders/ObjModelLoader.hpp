#pragma once
#include "../../Core/Engine.hpp"
#include <istream>
#include "../RenderInternal.hpp"

namespace worlds {
    void loadObj(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::istream& stream, LoadedMeshData& lmd);
}
