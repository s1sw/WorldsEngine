#include <Render/StandardPipeline/Bloom.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>

using namespace R2;

namespace worlds
{
    struct BloomPushConstants
    {
        uint32_t inputMipLevel;
        uint32_t inputArrayIdx;
        uint32_t outputW;
        uint32_t outputH;
    };

    Bloom::Bloom(VK::Core* core, VK::Texture* hdrSource)
        : core(core)
        , hdrSource(hdrSource)
    {
        VK::TextureCreateInfo tci = VK::TextureCreateInfo::Texture2D(VK::TextureFormat::B10G11R11_UFLOAT_PACK32, hdrSource->GetWidth(), hdrSource->GetHeight());
        tci.SetFullMipChain();

        if (hdrSource->GetLayers() > 1)
        {
            tci.Dimension = VK::TextureDimension::Array2D;
            tci.Layers = hdrSource->GetLayers();
        }

        if (tci.NumMips > 8) tci.NumMips = 8;

        mipChain = core->CreateTexture(tci);
        
        // Basically, the bloom works by:
        // 1. Copy the HDR source into the 0th mip of the chain
        // 2. Downsample each mip down the chain
        // 3. Upsample all the way back up
        // 4. Copy 0th mip to bloom target
        // Unfortunately, this is a pain to implement in Vulkan :(
        // We have to create texture views descriptor sets for each mip target to downsample to

        VK::SamplerBuilder sb{core};
        sb.AddressMode(VK::SamplerAddressMode::ClampToEdge);
        sb.MagFilter(VK::Filter::Linear).MinFilter(VK::Filter::Linear).MipmapMode(VK::SamplerMipmapMode::Linear);
        sampler = sb.Build();

        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        dslb.Binding(1, VK::DescriptorType::StorageImage, 1, VK::ShaderStage::Compute);
        dsl = dslb.Build();

        mipOutputViews.resize(tci.NumMips);
        mipOutputSets.resize(tci.NumMips);
        for (int i = 0; i < tci.NumMips; i++)
        {
            VK::TextureSubset texSubset{};
            texSubset.Dimension = tci.Layers > 1 ? VK::TextureDimension::Array2D : VK::TextureDimension::Dim2D;
            texSubset.LayerCount = tci.Layers;
            texSubset.LayerStart = 0;
            texSubset.MipCount = 1;
            texSubset.MipStart = i;

            mipOutputViews[i] = new VK::TextureView(core, mipChain.Get(), texSubset);

            mipOutputSets[i] = core->CreateDescriptorSet(dsl.Get());

            VK::DescriptorSetUpdater dsu{core, mipOutputSets[i].Get()};
            dsu.AddTextureWithLayout(0, 0, VK::DescriptorType::CombinedImageSampler, mipChain.Get(), VK::ImageLayout::General, sampler.Get());
            dsu.AddTextureView(1, 0, VK::DescriptorType::StorageImage, mipOutputViews[i].Get());
            dsu.Update();
        }

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Compute, 0, sizeof(BloomPushConstants));
        plb.DescriptorSet(dsl.Get());
        pipelineLayout = plb.Build();

        AssetID downsampleID = AssetDB::pathToId("Shaders/bloom_downsample.comp.spv");
        AssetID upsampleID = AssetDB::pathToId("Shaders/bloom_upsample.comp.spv");

        if (hdrSource->GetLayers() > 1)
        {
            downsampleID = AssetDB::pathToId("Shaders/bloom_downsample_multivp.comp.spv");
            upsampleID = AssetDB::pathToId("Shaders/bloom_upsample_multivp.comp.spv");
        }

        VK::ComputePipelineBuilder cpbDownsample{core};

        cpbDownsample.SetShader(ShaderCache::getModule(downsampleID));
        cpbDownsample.Layout(pipelineLayout.Get());
        downsample = cpbDownsample.Build();

        VK::ComputePipelineBuilder cpbUpsample{core};
        cpbUpsample.SetShader(ShaderCache::getModule(upsampleID));
        cpbUpsample.Layout(pipelineLayout.Get());
        upsample = cpbUpsample.Build();

        VK::ComputePipelineBuilder cpbSeed{core};
        AssetID seedShader;

        if (hdrSource->GetSamples() > 1)
        {
            if (hdrSource->GetLayers() > 1)
                seedShader = AssetDB::pathToId("Shaders/bloom_seed_msaa_multivp.comp.spv");
            else
                seedShader = AssetDB::pathToId("Shaders/bloom_seed_msaa.comp.spv");
        }
        else
            seedShader = AssetDB::pathToId("Shaders/bloom_seed.comp.spv");

        cpbSeed.SetShader(ShaderCache::getModule(seedShader));
        cpbSeed.Layout(pipelineLayout.Get());
        seedPipeline = cpbSeed.Build();

        seedDS = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater{core, seedDS.Get()}
            .AddTexture(0, 0, VK::DescriptorType::CombinedImageSampler, hdrSource, sampler.Get())
            .AddTexture(1, 0, VK::DescriptorType::StorageImage, mipChain.Get())
            .Update();
    }

    VK::Texture* Bloom::GetOutput()
    {
        return mipChain.Get();
    }

    int mipScale(int val, int mip)
    {
        int scaled = val >> mip;
        return scaled > 1 ? scaled : 1;
    }
    
    void Bloom::Execute(VK::CommandBuffer& cb)
    {
        cb.BeginDebugLabel("Bloom", 0.1f, 0.1f, 0.1f);

        mipChain->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderWrite | VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        hdrSource->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);

        cb.BindComputePipeline(seedPipeline.Get());
        cb.BindComputeDescriptorSet(pipelineLayout.Get(), seedDS.Get(), 0);
        cb.Dispatch((hdrSource->GetWidth() + 15) / 16, (hdrSource->GetHeight() + 15) / 16, hdrSource->GetLayers());

        // Downsample time!
        cb.BindComputePipeline(downsample.Get());
        cb.BeginDebugLabel("Bloom Layer", 0.1f, 0.1f, 0.1f);
        int w = mipChain->GetWidth() / 2;
        int h = mipChain->GetHeight() / 2;
        for (int mip = 1; mip < mipChain->GetNumMips(); mip++)
        {
            cb.TextureBarrier(mipChain.Get(), VK::PipelineStageFlags::ComputeShader, VK::PipelineStageFlags::ComputeShader,
                VK::AccessFlags::ShaderReadWrite, VK::AccessFlags::ShaderReadWrite);

            // We want to downsample from the previous mip level to this mip level.
            BloomPushConstants pcs { (uint32_t)(mip - 1), (uint32_t)0, (uint32_t)w, (uint32_t)h };
            cb.PushConstants(pcs, VK::ShaderStage::Compute, pipelineLayout.Get());
            cb.BindComputeDescriptorSet(pipelineLayout.Get(), mipOutputSets[mip].Get(), 0);
            cb.Dispatch((w + 15) / 16, (h + 15) / 16, hdrSource->GetLayers());

            w /= 2;
            h /= 2;
        }

        // Now upsample!
        cb.BindComputePipeline(upsample.Get());
        for (int mip = mipChain->GetNumMips() - 1; mip > 0; mip--)
        {
            cb.TextureBarrier(mipChain.Get(), VK::PipelineStageFlags::ComputeShader, VK::PipelineStageFlags::ComputeShader,
                VK::AccessFlags::ShaderReadWrite, VK::AccessFlags::ShaderReadWrite);

            int w = mipScale(mipChain->GetWidth(), mip - 1);
            int h = mipScale(mipChain->GetHeight(), mip - 1);

            // We want to upsample from this mip level to the next mip level.
            BloomPushConstants pcs { (uint32_t)mip, (uint32_t)0, (uint32_t)w, (uint32_t)h };
            cb.PushConstants(pcs, VK::ShaderStage::Compute, pipelineLayout.Get());
            cb.BindComputeDescriptorSet(pipelineLayout.Get(), mipOutputSets[mip - 1].Get(), 0);
            cb.Dispatch((w + 15) / 16, (h + 15) / 16, hdrSource->GetLayers());
        }
        cb.EndDebugLabel();

        cb.EndDebugLabel();
    }
}