#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKTexture.hpp>
#include <Render/MaterialManager.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderReflector.hpp>
#include <Render/ShaderCache.hpp>
#include <entt/entity/registry.hpp>

using namespace R2;

namespace worlds
{
    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
        uint32_t textureID;
    };

    StandardPipeline::StandardPipeline(VKRenderer* renderer) : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();
        
        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffer = core->CreateBuffer(modelMatrixBci);

        AssetID vs = AssetDB::pathToId("Shaders/standard.vert.spv");
        AssetID fs = AssetDB::pathToId("Shaders/standard.frag.spv");

        ShaderReflector sr{vs};

        descriptorSetLayout = sr.createDescriptorSetLayout(core, 0);
        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout);

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet};
        sr.bindBuffer(dsu, "VPBuffer_0", multiVPBuffer);
        sr.bindBuffer(dsu, "ModelMatrices_0", modelMatrixBuffer);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(descriptorSetLayout)
            .DescriptorSet(&renderer->getBindlessTextureManager()->GetTextureDescriptorSetLayout());
        pipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, position));
        vb.Attributes.emplace_back(1, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, normal));
        vb.Attributes.emplace_back(2, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, tangent));
        vb.Attributes.emplace_back(3, VK::TextureFormat::R32_SFLOAT, offsetof(Vertex, bitangentSign));
        vb.Attributes.emplace_back(4, VK::TextureFormat::R32G32_SFLOAT, offsetof(Vertex, uv));

        VK::ShaderModule& stdVert = ShaderCache::getModule(vs);
        VK::ShaderModule& stdFrag = ShaderCache::getModule(fs);

        VK::PipelineBuilder pb{core->GetHandles()};
        pb.PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout)
            .ColorAttachmentFormat(VK::TextureFormat::R8G8B8A8_SRGB)
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, stdFrag)
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT);

        pipeline = pb.Build();
    }

    StandardPipeline::~StandardPipeline()
    {
        // todo
        delete pipeline;
        delete pipelineLayout;
        delete multiVPBuffer;
        delete modelMatrixBuffer;
    }

    void StandardPipeline::setup(VKRTTPass* rttPass)
    {
        VK::Core* core = renderer->getCore();
        this->rttPass = rttPass;

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::R16G16B16A16_SFLOAT, rttPass->width, rttPass->height);
        colorBuffer = core->CreateTexture(colorBufferCI);
    }

    void StandardPipeline::onResize(int width, int height)
    {
        VK::Core* core = renderer->getCore();
        core->DestroyTexture(depthBuffer);
        core->DestroyTexture(colorBuffer);

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::R16G16B16A16_SFLOAT, rttPass->width, rttPass->height);
        colorBuffer = core->CreateTexture(colorBufferCI);
    }

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        MaterialManager* materialManager = renderer->getMaterialManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrixBuffer->Map();

        VK::RenderPass r;
        r.ColorAttachment(rttPass->getFinalTarget(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(1.f, 0.f, 1.f, 1.f))
            .DepthAttachment(depthBuffer, VK::LoadOp::Clear, VK::StoreOp::Store)
            .DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f))
            .RenderArea(rttPass->width, rttPass->height)
            .Begin(cb);

        Camera* camera = rttPass->getCamera();

        MultiVP multiVPs{};
        multiVPs.views[0] = camera->getViewMatrix();
        multiVPs.projections[0] = camera->getProjectionMatrix((float)rttPass->width / rttPass->height);

        core->QueueBufferUpload(multiVPBuffer, &multiVPs, sizeof(multiVPs), 0);

        uint32_t modelMatrixIndex = 0;

        cb.BindPipeline(pipeline);
        cb.BindGraphicsDescriptorSet(pipelineLayout, descriptorSet->GetNativeHandle(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout, btm->GetTextureDescriptorSet().GetNativeHandle(), 1);
        cb.SetViewport(VK::Viewport::Simple((float)rttPass->width, (float)rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t)
        {
            RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

            modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();
            cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), rmi.vertsOffset);

            VK::IndexType vkIdxType =
                rmi.indexType == IndexType::Uint32 ? VK::IndexType::Uint32 : VK::IndexType::Uint16;
            cb.BindIndexBuffer(meshManager->getIndexBuffer(), rmi.indexOffset, vkIdxType);

            for (int i = 0; i < rmi.numSubmeshes; i++)
            {
                RenderSubmeshInfo& rsi = rmi.submeshInfo[i];

                StandardPushConstants spc{};
                spc.modelMatrixID = modelMatrixIndex-1;
                uint8_t materialIndex = rsi.materialIndex;

                if (!wo.presentMaterials[materialIndex])
                {
                    materialIndex = 0;
                }

                AssetID albedoId = AssetDB::pathToId(materialManager->loadOrGet(wo.materials[materialIndex])["albedoPath"]);
                spc.textureID = textureManager->loadOrGet(albedoId);
                cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout);

                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });
        r.End(cb);

        modelMatrixBuffer->Unmap();
    }
}