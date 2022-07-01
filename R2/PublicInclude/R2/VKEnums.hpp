#pragma once

namespace R2::VK
{
    enum class ShaderStage
    {
        Vertex = 0x1,
        Fragment = 0x10,
        Compute = 0x20,
        AllRaster = Vertex | Fragment
    };

    inline ShaderStage operator|(const ShaderStage& a, const ShaderStage& b)
    {
        return static_cast<ShaderStage>((uint32_t)a | (uint32_t)b);
    }
}