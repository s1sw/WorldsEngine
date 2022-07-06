#include <Render/FakeLitPipeline.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <Render/ShaderReflector.hpp>
#include <entt/entity/registry.hpp>

using namespace R2;

namespace worlds {
    struct StandardPushConstants {
        uint32_t modelMatrixID;
    };

    FakeLitPipeline::FakeLitPipeline(VKRenderer* renderer) 
        : renderer(renderer) {
        VK::Core* core = renderer->getCore();

        VK::BufferCreateInfo vpBci{ VK::BufferUsage::Uniform, sizeof(MultiVP), true };
        multiVPBuffer = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{ VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true };
        modelMatrixBuffer = core->CreateBuffer(modelMatrixBci);

        ShaderReflector sr{AssetDB::pathToId("Shaders/standard.vert.spv")};

        descriptorSetLayout = sr.createDescriptorSetLayout(core, 0);
        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout);

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet};
        sr.bindBuffer(dsu, "VPBuffer_0", multiVPBuffer);
        sr.bindBuffer(dsu, "ModelMatrices_0", modelMatrixBuffer);
        dsu.Update();
        
        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb
            .PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(descriptorSetLayout);
        standardPipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        vb.Attributes.push_back(VK::VertexAttribute{0, VK::TextureFormat::R32G32B32_SFLOAT, 0});
        vb.Attributes.push_back(VK::VertexAttribute{1, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(Vertex, normal)});

        VK::ShaderModule& stdVert = ShaderCache::getModule(AssetDB::pathToId("Shaders/standard.vert.spv"));
        VK::ShaderModule& stdFrag = ShaderCache::getModule(AssetDB::pathToId("Shaders/standard.frag.spv"));

        VK::PipelineBuilder pb{core->GetHandles()};
        pb
            .PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(standardPipelineLayout)
            .ColorAttachmentFormat(VK::TextureFormat::R8G8B8A8_SRGB)
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, stdFrag)
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT);
        
        standardPipeline = pb.Build();
    }

    FakeLitPipeline::~FakeLitPipeline() {
        VK::Core* core = renderer->getCore();
        core->DestroyBuffer(multiVPBuffer);
        core->DestroyBuffer(modelMatrixBuffer);
        delete standardPipeline;
        // TODO: destroy pipeline layout
    }

    void FakeLitPipeline::setup(VKRTTPass* rttPass) {
        this->rttPass = rttPass;
        VK::TextureCreateInfo depthBufferCI = VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = renderer->getCore()->CreateTexture(depthBufferCI);
    }

    void FakeLitPipeline::onResize(int width, int height) {
        renderer->getCore()->DestroyTexture(depthBuffer);
        VK::TextureCreateInfo depthBufferCI = VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = renderer->getCore()->CreateTexture(depthBufferCI);
    }

    void FakeLitPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb) {
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrixBuffer->Map();

        VK::RenderPass r;
        r   .ColorAttachment(rttPass->getFinalTarget(), VK::LoadOp::Clear, VK::StoreOp::Store)
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

        cb.BindPipeline(standardPipeline);
        cb.BindGraphicsDescriptorSet(standardPipelineLayout, descriptorSet->GetNativeHandle(), 0);
        cb.SetViewport(VK::Viewport::Simple(rttPass->width, rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

            StandardPushConstants spc{};
            spc.modelMatrixID = modelMatrixIndex;
            cb.PushConstants(spc, VK::ShaderStage::AllRaster, standardPipelineLayout);

            modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();

            cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), rmi.vertsOffset);

            VK::IndexType vkIdxType = rmi.indexType == IndexType::Uint32 ? VK::IndexType::Uint32 : VK::IndexType::Uint16;
            cb.BindIndexBuffer(meshManager->getIndexBuffer(), rmi.indexOffset, vkIdxType);
            
            for (int i = 0; i < rmi.numSubmeshes; i++) {
                RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });
        r.End(cb);

        modelMatrixBuffer->Unmap();
    }
}