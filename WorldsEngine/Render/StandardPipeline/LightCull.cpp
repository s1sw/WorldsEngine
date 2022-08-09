#include <Render/StandardPipeline/LightCull.hpp>
#include <Core/AssetDB.hpp>
#include <glm/mat4x4.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/SimpleCompute.hpp>

using namespace R2;

namespace worlds
{
    struct LightCullPushConstants
    {
        uint32_t eyeIndex;
    };

    LightCull::LightCull(VK::Core* core, VK::Texture* depthBuffer, VK::Buffer* lightBuffer, VK::Buffer* lightTiles, VK::Buffer* multiVPBuffer)
        : core(core)
        , depthBuffer(depthBuffer)
        , lightBuffer(lightBuffer)
        , lightTiles(lightTiles)
        , multiVPBuffer(multiVPBuffer)
    {
        VK::SamplerBuilder sb{core};
        sampler = sb.Build();

        std::string shaderPath = "Shaders/light_cull";

        if (depthBuffer->GetSamples() != 1)
        {
            // MSAA
            shaderPath += "_msaa.comp.spv";
        }
        else
        {
            shaderPath += ".comp.spv";
        }

        AssetID lightCullShader = AssetDB::pathToId(shaderPath);

        cs = new SimpleCompute(core, lightCullShader);
        cs->BindUniformBuffer(0, multiVPBuffer);
        cs->BindSampledTexture(1, depthBuffer, sampler.Get());
        cs->BindStorageBuffer(2, lightBuffer);
        cs->BindStorageBuffer(3, lightTiles);
        cs->PushConstantSize(sizeof(LightCullPushConstants));
        cs->Build();
    }

    void LightCull::Execute(VK::CommandBuffer& cb)
    {
        depthBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderSampledRead);
        lightTiles->Acquire(cb, VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);
        LightCullPushConstants pcs{};
        pcs.eyeIndex = 0;

        int w = depthBuffer->GetWidth();
        int h = depthBuffer->GetHeight();
        cs->Dispatch(cb, pcs, (w + 15) / 16, (h + 15) / 16, 1);
    }
}