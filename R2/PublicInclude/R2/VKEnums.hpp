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
        return static_cast<ShaderStage>((unsigned int)a | (unsigned int)b);
    }

    enum class CompareOp : unsigned int
    {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessOrEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterOrEqual = 6,
        Always = 7
    };
}