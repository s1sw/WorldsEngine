#include <Render/FakeLitPipeline.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKTexture.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/ShaderReflector.hpp>
#include <entt/entity/registry.hpp>

using namespace R2;

namespace worlds
{
    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
    };

    FakeLitPipeline::FakeLitPipeline(VKRenderer* renderer) : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();

        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffer = core->CreateBuffer(modelMatrixBci);

        AssetID vs = AssetDB::pathToId("Shaders/fake_lit.vert.spv");
        AssetID fs = AssetDB::pathToId("Shaders/fake_lit.frag.spv");

        ShaderReflector sr{vs};

        descriptorSetLayout = sr.createDescriptorSetLayout(core, 0);
        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout);

        VK::DescriptorSetUpdater dsu{core, descriptorSet};
        sr.bindBuffer(dsu, "VPBuffer_0", multiVPBuffer);
        sr.bindBuffer(dsu, "ModelMatrices_0", modelMatrixBuffer);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(descriptorSetLayout);
        pipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        // TODO: This doesn't work because Slang doesn't preserve vertex attribute names
        //sr.bindVertexAttribute(vb, "Position", VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, position));
        //sr.bindVertexAttribute(vb, "Normal", VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, normal));

        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
        vb.Attributes.emplace_back(1, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));

        VK::ShaderModule& stdVert = ShaderCache::getModule(vs);
        VK::ShaderModule& stdFrag = ShaderCache::getModule(fs);

        VK::PipelineBuilder pb{core};
        pb.PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout)
            .ColorAttachmentFormat(VK::TextureFormat::R8G8B8A8_SRGB)
            .AddVertexBinding(vb)
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, stdFrag)
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT);

        pipeline = pb.Build();
    }

    FakeLitPipeline::~FakeLitPipeline()
    {
        VK::Core* core = renderer->getCore();
        core->DestroyBuffer(multiVPBuffer);
        core->DestroyBuffer(modelMatrixBuffer);
        delete pipeline;
        delete pipelineLayout;
    }

    void FakeLitPipeline::setup(VKRTTPass* rttPass)
    {
        this->rttPass = rttPass;
        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = renderer->getCore()->CreateTexture(depthBufferCI);
    }

    void FakeLitPipeline::onResize(int width, int height)
    {
        renderer->getCore()->DestroyTexture(depthBuffer);
        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = renderer->getCore()->CreateTexture(depthBufferCI);
    }

    void FakeLitPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();

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
        cb.BindGraphicsDescriptorSet(pipelineLayout, descriptorSet, 0);
        cb.SetViewport(VK::Viewport::Simple(rttPass->width, rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

            StandardPushConstants spc{};
            spc.modelMatrixID = modelMatrixIndex;
            cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout);

            modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();

            cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), rmi.vertsOffset);
            cb.BindIndexBuffer(meshManager->getIndexBuffer(), rmi.indexOffset, VK::IndexType::Uint32);

            for (int i = 0; i < rmi.numSubmeshes; i++)
            {
                RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });
        r.End(cb);

        modelMatrixBuffer->Unmap();
    }
}