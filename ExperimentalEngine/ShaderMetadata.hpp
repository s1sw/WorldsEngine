#pragma once
#include <cstdint>

class ShaderMetadata {

};

ShaderMetadata generateSpirvMetadata(uint32_t* data, size_t length);