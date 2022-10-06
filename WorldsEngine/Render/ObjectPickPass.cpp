#include "ObjectPickPass.hpp"
#include <Core/AssetDB.hpp>
#include <Core/WorldComponents.hpp>
#include <entt/entity/registry.hpp>
#include <Render/CullMesh.hpp>
#include <Render/Frustum.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/VK.hpp>
#include <R2/VKSyncPrims.hpp>

using namespace R2;

namespace worlds
{
    struct PickingBuffer
    {
        uint32_t entityId;
    };

    struct PickPushConstants
    {
        glm::mat4 mvp;
        uint32_t pixelX;
        uint32_t pixelY;
        uint32_t entityId;
    };

    ObjectPickPass::ObjectPickPass(VKRenderer* renderer)
        : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();

        VK::TextureCreateInfo dtci = VK::TextureCreateInfo::RenderTarget2D(
            VK::TextureFormat::D32_SFLOAT, 256, 256
        );
        depthTex = core->CreateTexture(dtci);

        VK::BufferCreateInfo bci{ VK::BufferUsage::Storage, sizeof(PickingBuffer), true };
        pickBuffer = core->CreateBuffer(bci);

        VK::DescriptorSetLayoutBuilder dslb{renderer->getCore()};
        dslb.Binding(0, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Fragment);
        dsl = dslb.Build();

        ds = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater dsu{core, ds.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::StorageBuffer, pickBuffer.Get());
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core};
        plb
            .DescriptorSet(dsl.Get())
            .PushConstants(VK::ShaderStage::AllRaster, 0, sizeof(PickPushConstants));

        pickPipelineLayout = plb.Build();

        VK::VertexBinding vertBinding{};
        vertBinding.Size = sizeof(Vertex);
        vertBinding.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, 0);

        VK::ShaderModule& fs = ShaderCache::getModule("Shaders/pick.frag.spv");
        VK::ShaderModule& vs = ShaderCache::getModule("Shaders/pick.vert.spv");

        VK::PipelineBuilder pb{core};
        pb
            .Layout(pickPipelineLayout.Get())
            .AddVertexBinding(std::move(vertBinding))
            .Layout(pickPipelineLayout.Get())
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .CullMode(VK::CullMode::Back)
            .PrimitiveTopology(VK::Topology::TriangleList)
            .AddShader(VK::ShaderStage::Vertex, vs)
            .AddShader(VK::ShaderStage::Fragment, fs);

        pickPipeline = pb.Build();

        pickEvent = new VK::Event(core);

        params.screenWidth = 256;
        params.screenHeight = 256;
        params.cam = nullptr;
    }

    void ObjectPickPass::requestPick(PickParams params)
    {
        pickEvent->Set();
        this->params = params;
        doPick = true;
    }

    void ObjectPickPass::execute(VK::CommandBuffer& cb, entt::registry &reg)
    {
        if (!doPick) return;
        if (params.cam == nullptr) return;
        if (depthTex->GetWidth() != params.screenWidth || depthTex->GetHeight() != params.screenHeight)
        {
            VK::Core* core = renderer->getCore();
            VK::TextureCreateInfo dtci = VK::TextureCreateInfo::RenderTarget2D(
                    VK::TextureFormat::D32_SFLOAT, params.screenWidth, params.screenHeight
            );
            depthTex = core->CreateTexture(dtci);
        }

        RenderMeshManager* meshManager = renderer->getMeshManager();

        cb.BeginDebugLabel("Picking Pass", 0.658f, 0.196f, 0.267f);
        cb.BindPipeline(pickPipeline.Get());
        cb.BindGraphicsDescriptorSet(pickPipelineLayout.Get(), ds.Get(), 0);
        cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), 0);
        cb.BindIndexBuffer(meshManager->getIndexBuffer(), 0, VK::IndexType::Uint32);
        cb.SetViewport(VK::Viewport::Simple(params.screenWidth, params.screenHeight));
        cb.SetScissor(VK::ScissorRect::Simple(params.screenWidth, params.screenHeight));

        // clear picking buffer
        PickingBuffer empty{entt::null};
        pickBuffer->Acquire(cb, VK::AccessFlags::TransferWrite, VK::PipelineStageFlags::Transfer);
        cb.UpdateBuffer(pickBuffer.Get(), 0, sizeof(PickingBuffer), &empty);

        pickBuffer->Acquire(cb, VK::AccessFlags::ShaderStorageWrite, VK::PipelineStageFlags::FragmentShader);
        VK::RenderPass rp{};
        rp.RenderArea(params.screenWidth, params.screenHeight);
        rp.DepthAttachment(depthTex.Get(), VK::LoadOp::Clear, VK::StoreOp::Store);
        rp.DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f));

        rp.Begin(cb);

        float aspect = (float)params.screenWidth / (float)params.screenHeight;
        glm::mat4 vp = params.cam->getProjectionMatrix(aspect) * params.cam->getViewMatrix();
        Frustum frustum{};
        frustum.fromVPMatrix(vp);

        reg.view<WorldObject, Transform>().each(
            [&](entt::entity entity, WorldObject& wo, Transform& woT) {
                const RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

                if (!cullMesh(rmi, woT, &frustum, 1))
                    return;

                PickPushConstants pcs{};
                pcs.mvp = vp * woT.getMatrix();
                pcs.pixelX = params.pickX;
                pcs.pixelY = params.pickY;
                pcs.entityId = (uint32_t)entity;
                cb.PushConstants(pcs, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment,
                                 pickPipelineLayout.Get());

                for (int i = 0; i < rmi.numSubmeshes; i++)
                {
                    if (!wo.drawSubmeshes[i]) continue;

                    const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                    cb.DrawIndexed(
                        rsi.indexCount,
                        1,
                        rsi.indexOffset + (rmi.indexOffset / sizeof(uint32_t)),
                        (int)rmi.vertsOffset / (int)sizeof(Vertex),
                        0
                    );
                }
            }
        );

        rp.End(cb);

        cb.ResetEvent(pickEvent.Get());
        cb.EndDebugLabel();

        doPick = false;
    }

    bool ObjectPickPass::getResult(uint32_t& entityID)
    {
        if (pickEvent->IsSet())
            return false;

        auto buf = (PickingBuffer*)pickBuffer->Map();
        entityID = buf->entityId;
        pickBuffer->Unmap();

        return true;
    }
}