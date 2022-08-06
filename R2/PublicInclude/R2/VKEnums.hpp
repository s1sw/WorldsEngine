#pragma once
#include <stdint.h>

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

    enum class AccessFlags : uint64_t
    {
        None = 0,
        IndirectCommandRead = 0x00000001ULL,
        IndexRead = 0x00000002ULL,
        VertexAttributeRead = 0x00000004ULL,
        UniformRead = 0x00000008ULL,
        InputAttachmentRead = 0x00000010ULL,
        ShaderRead = 0x00000020ULL,
        ShaderWrite = 0x00000040ULL,
        ColorAttachmentRead = 0x00000080ULL,
        ColorAttachmentWrite = 0x00000100ULL,
        ColorAttachmentReadWrite = ColorAttachmentRead | ColorAttachmentWrite,
        DepthStencilAttachmentRead = 0x00000200ULL,
        DepthStencilAttachmentWrite = 0x00000400ULL,
        DepthStencilAttachmentReadWrite = DepthStencilAttachmentRead | DepthStencilAttachmentWrite,
        TransferRead = 0x00000800ULL,
        TransferWrite = 0x00001000ULL,
        HostRead = 0x00002000ULL,
        HostWrite = 0x00004000ULL,
        MemoryRead = 0x00008000ULL,
        MemoryWrite = 0x00010000ULL,
        ShaderSampledRead = 0x100000000ULL,
        ShaderStorageRead = 0x200000000ULL,
        ShaderStorageWrite = 0x400000000ULL
    };

    inline AccessFlags operator|(const AccessFlags& a, const AccessFlags& b)
    {
        return static_cast<AccessFlags>((unsigned int)a | (unsigned int)b);
    }

    enum class PipelineStageFlags : uint64_t
    {
        None = 0ULL,
        TopOfPipe = 0x00000001ULL,
        DrawIndirect = 0x00000002ULL,
        VertexInput = 0x00000004ULL,
        VertexShader = 0x00000008ULL,
        TesselationControlShader = 0x00000010ULL,
        TesselationEvaluationShader = 0x00000020ULL,
        GeometryShader = 0x00000040ULL,
        FragmentShader = 0x00000080ULL,
        EarlyFragmentTests = 0x00000100ULL,
        LateFragmentTests = 0x00000200ULL,
        ColorAttachmentOutput = 0x00000400ULL,
        ComputeShader = 0x00000800ULL,
        AllTransfer = 0x00001000ULL,
        Transfer = 0x00001000ULL,
        BottomOfPipe = 0x00002000ULL,
        Host = 0x00004000ULL,
        AllGraphics = 0x00008000ULL,
        AllCommands = 0x00010000ULL,
        Copy = 0x100000000ULL,
        Resolve = 0x200000000ULL,
        Blit = 0x400000000ULL,
        Clear = 0x800000000ULL,
        IndexInput = 0x1000000000ULL,
        VertexAttributeInput = 0x2000000000ULL,
        PreRasterizationShaders = 0x4000000000ULL,
    };

    inline PipelineStageFlags operator|(const PipelineStageFlags& a, const PipelineStageFlags& b)
    {
        return static_cast<PipelineStageFlags>((unsigned int)a | (unsigned int)b);
    }

    inline PipelineStageFlags operator|=(PipelineStageFlags& a, const PipelineStageFlags& b)
    {
        return a = a | b;
    }
}