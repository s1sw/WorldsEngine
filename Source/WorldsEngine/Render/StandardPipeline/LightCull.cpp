#include <Render/StandardPipeline/LightCull.hpp>
#include <Core/AssetDB.hpp>
#include <glm/mat4x4.hpp>
#include <R2/VK.hpp>
#include <R2/VKTimestampPool.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/SimpleCompute.hpp>

using namespace R2;

namespace worlds
{
    struct LightCullPushConstants
    {
        uint32_t eyeIndex;
    };

    LightCull::LightCull(VKRenderer* renderer, VK::Texture* depthBuffer, VK::FrameSeparatedBuffer* lightBuffers, VK::Buffer* lightTiles, VK::Buffer* multiVPBuffer)
        : renderer(renderer)
        , depthBuffer(depthBuffer)
        , lightBuffers(lightBuffers)
        , lightTiles(lightTiles)
        , multiVPBuffer(multiVPBuffer)
    {
        VK::Core* core = renderer->getCore();
        VK::SamplerBuilder sb{core};
        sampler = sb.Build();

        std::string shaderPath = "Shaders/light_cull";

        if (depthBuffer->GetSamples() != 1)
        {
            // MSAA
            shaderPath += "_msaa";
        }

        if (depthBuffer->GetLayers() != 1)
        {
            shaderPath += "_multivp";
        }

        shaderPath += ".comp.spv";

        AssetID lightCullShader = AssetDB::pathToId(shaderPath);

        for (int i = 0; i < core->GetNumFramesInFlight(); i++)
        {
            cs[i] = new SimpleCompute(core, lightCullShader);
            cs[i]->BindUniformBuffer(0, multiVPBuffer);
            cs[i]->BindSampledTexture(1, depthBuffer, sampler.Get());
            cs[i]->BindStorageBuffer(2, lightBuffers->GetBuffer(i));
            cs[i]->BindStorageBuffer(3, lightTiles);
            cs[i]->PushConstantSize(sizeof(LightCullPushConstants));
            cs[i]->Build();
        }
    }

    void LightCull::Execute(VK::CommandBuffer& cb)
    {
        VK::Core* core = renderer->getCore();

        depthBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        lightTiles->Acquire(cb, VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);
        LightCullPushConstants pcs{};
        pcs.eyeIndex = 0;

        int w = depthBuffer->GetWidth();
        int h = depthBuffer->GetHeight();
        cs[core->GetFrameIndex()]->Dispatch(cb, pcs, (w + 31) / 32, (h + 31) / 32, 1);

        if (depthBuffer->GetLayers() == 2)
        {
            pcs.eyeIndex = 1;

            int w = depthBuffer->GetWidth();
            int h = depthBuffer->GetHeight();
            cs[core->GetFrameIndex()]->Dispatch(cb, pcs, (w + 31) / 32, (h + 31) / 32, 1);
        }
    }
}