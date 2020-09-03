#pragma once
#include "Engine.hpp"
#include <istream>
#include "Render.hpp"

namespace worlds {
    void loadObj(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::istream& stream);
}