#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class ShaderType {
    Fragment,
    Vertex
};

struct ShaderVar {
    std::string name;
    uint8_t vectorWidth;
    uint8_t bitWidth;
    uint8_t location;

    bool operator!=(ShaderVar& other) {
        return vectorWidth != other.vectorWidth && bitWidth != other.bitWidth && location != other.location;
    }
};

struct ShaderMetadata {
    std::string sourceFile;
    ShaderType type;
    std::optional<uint32_t> modelMatrixBinding;
    std::optional<uint32_t> lightBufferBinding;
    bool valid;
    std::optional<uint32_t> viewPosPCOffset;
    std::optional<uint32_t> texScaleOffsetPCOffset;
    std::optional<uint32_t> ubIndicesPCOffset;
    std::vector<ShaderVar> inputVars;
    std::vector<ShaderVar> outputVars;
};

ShaderMetadata generateSpirvMetadata(uint32_t* data, size_t length);
bool shadersCompatible(ShaderMetadata& vs, ShaderMetadata& fs);