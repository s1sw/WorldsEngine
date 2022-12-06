#include <Render/StandardPipeline/CubemapConvoluter.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VK.hpp>
#include <Render/SimpleCompute.hpp>

using namespace R2;

namespace worlds
{
    CubemapConvoluter::CubemapConvoluter(R2::VK::Core* core)
        : core(core)
    {
        VK::SamplerBuilder sb{core};
        sb.AddressMode(VK::SamplerAddressMode::Repeat)
        .MagFilter(VK::Filter::Linear)
        .MinFilter(VK::Filter::Linear)
        .MipmapMode(VK::SamplerMipmapMode::Linear);
        sampler = sb.Build();
    }

    struct CCPushConstants
    {
        float roughness;
        int faceIdx;
        int outputIsSRGB;
    };

    void CubemapConvoluter::Convolute(R2::VK::CommandBuffer cb, R2::VK::Texture* tex)
    {
        if (tex->GetFormat() != VK::TextureFormat::R32G32B32A32_SFLOAT && tex->GetFormat() != VK::TextureFormat::R8G8B8A8_SRGB)
        {
            return;
        }

        tex->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderRead | VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);

        bool isSRGB = tex->GetFormat() == VK::TextureFormat::R8G8B8A8_SRGB;
        int w = tex->GetWidth() / 2;
        int h = tex->GetHeight() / 2;
        for (int mipLevel = 1; mipLevel < tex->GetNumMips(); mipLevel++)
        {
            for (int i = 0; i < 6; i++)
            {
                tex->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderRead | VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);
                SimpleCompute sc{core, AssetDB::pathToId("Shaders/cubemap_convolute.comp.spv")};

                VK::TextureSubset subset{};
                subset.Dimension = VK::TextureDimension::Dim2D;
                subset.LayerCount = 1;
                subset.LayerStart = i;
                subset.MipCount = 1;
                subset.MipStart = mipLevel;

                UniquePtr<VK::TextureView> texView = new VK::TextureView(core, tex, subset);
                sc.PushConstantSize(sizeof(CCPushConstants));
                sc.BindSampledTextureWithLayout(0, tex, R2::VK::ImageLayout::General, sampler.Get());
                sc.BindStorageTextureView(1, texView.Get());
                sc.Build();

                CCPushConstants pcs{};
                pcs.faceIdx = i;
                pcs.roughness = (float)mipLevel / tex->GetNumMips();
                pcs.outputIsSRGB = isSRGB;
                sc.Dispatch(cb, pcs, (w + 15) / 16, (h + 15) / 16, 1);
            }

            w /= 2;
            h /= 2;
        }
        tex->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderRead | VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);
    }
}