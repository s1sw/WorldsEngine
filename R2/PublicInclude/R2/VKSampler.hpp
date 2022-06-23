#pragma once

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkSampler)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;
    typedef unsigned int Bool32;

    class Sampler
    {
    public:
        Sampler(const Handles* handles, VkSampler sampler);
        VkSampler GetNativeHandle();
        ~Sampler();
    private:
        VkSampler sampler;
        const Handles* handles;
    };

    enum class Filter : unsigned int
    {
        Nearest = 0,
        Linear = 1,
    };

    enum class SamplerMipmapMode : unsigned int
    {
        Nearest = 0,
        Linear = 1
    };

    enum class SamplerAddressMode : unsigned int
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirrorClampToEdge = 4
    };

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

    enum class BorderColor : unsigned int
    {
        FloatTransparentBlack = 0,
        IntTransparentBlack = 1,
        FloatOpaqueBlack = 2,
        IntOpaqueBlack = 3,
        FloatOpaqueWhite = 4,
        IntOpaqueWhite = 5
    };

    class SamplerBuilder
    {
    public:
        SamplerBuilder(const Handles* handles);

        SamplerBuilder& MagFilter(Filter filt);
        SamplerBuilder& MinFilter(Filter filt);
        SamplerBuilder& MipmapMode(SamplerMipmapMode mode);
        SamplerBuilder& AddressMode(SamplerAddressMode mode);

        Sampler* Build();
    private:
        struct SamplerCreateInfo
        {
            Filter                magFilter;
            Filter                minFilter;
            SamplerMipmapMode     mipmapMode;
            SamplerAddressMode    addressModeU;
            SamplerAddressMode    addressModeV;
            SamplerAddressMode    addressModeW;
            float                 mipLodBias;
            Bool32                anisotropyEnable;
            float                 maxAnisotropy;
            Bool32                compareEnable;
            CompareOp             compareOp;
            float                 minLod;
            float                 maxLod;
            BorderColor           borderColor;
            Bool32                unnormalizedCoordinates;
        };

        SamplerCreateInfo ci;
        const Handles* handles;
    };
}