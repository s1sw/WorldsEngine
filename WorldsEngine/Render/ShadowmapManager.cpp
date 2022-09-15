#include <Render/RenderInternal.hpp>
#include <Core/AssetDB.hpp>
#include <entt/entity/registry.hpp>
#include <Render/CullMesh.hpp>
#include <Render/Frustum.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VK.hpp>

using namespace R2;

namespace worlds
{
    const int SHADOWMAP_RES = 1024;
    ShadowmapManager::ShadowmapManager(VKRenderer* renderer) : renderer(renderer)
    {
        VK::PipelineLayoutBuilder plb{renderer->getCore()->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Vertex, 0, sizeof(glm::mat4));
        pipelineLayout = plb.Build();

        VK::VertexBinding vertBinding{};
        vertBinding.Size = sizeof(Vertex);
        vertBinding.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, 0);

        VK::PipelineBuilder pb{renderer->getCore()};
        pb.AddShader(VK::ShaderStage::Vertex, ShaderCache::getModule(AssetDB::pathToId("Shaders/shadowmap.vert.spv")))
            .AddShader(VK::ShaderStage::Fragment, ShaderCache::getModule(AssetDB::pathToId("Shaders/blank.frag.spv")))
            .AddVertexBinding(std::move(vertBinding))
            .Layout(pipelineLayout.Get())
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .CullMode(VK::CullMode::Back)
            .PrimitiveTopology(VK::Topology::TriangleList)
            .DepthBias(true)
            .ConstantDepthBias(-1.0f)
            .SlopeDepthBias(-1.75f);

        pipeline = pb.Build();

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++)
        {
            VK::TextureCreateInfo tci =
                VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, SHADOWMAP_RES, SHADOWMAP_RES);
            ShadowmapInfo si{};
            si.texture = renderer->getCore()->CreateTexture(tci);
            si.bindlessID = renderer->getBindlessTextureManager()->AllocateTextureHandle(si.texture.Get());
            shadowmapInfo.push_back(std::move(si));
        }
    }

    void ShadowmapManager::AllocateShadowmaps(entt::registry& registry)
    {
        uint32_t count = 0;
        registry.view<WorldLight>().each([&](WorldLight& worldLight) {
            if (!worldLight.enableShadows || worldLight.type != LightType::Spot || count == NUM_SHADOW_LIGHTS)
            {
                worldLight.shadowmapIdx = ~0u;
                return;
            }

            worldLight.shadowmapIdx = count++;
        });
    }

    void ShadowmapManager::RenderShadowmaps(R2::VK::CommandBuffer& cb, entt::registry& registry)
    {
        RenderMeshManager* meshManager = renderer->getMeshManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        cb.BeginDebugLabel("Shadows", 0.1f, 0.1f, 0.1f);

        cb.BindPipeline(pipeline.Get());
        cb.SetScissor(VK::ScissorRect::Simple(SHADOWMAP_RES, SHADOWMAP_RES));
        cb.SetViewport(VK::Viewport::Simple(SHADOWMAP_RES, SHADOWMAP_RES));
        cb.BindIndexBuffer(meshManager->getIndexBuffer(), 0, VK::IndexType::Uint32);
        cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), 0);

        registry.view<WorldLight, Transform>().each([&](WorldLight& worldLight, Transform& t) {
            if (!worldLight.enableShadows || worldLight.type != LightType::Spot || worldLight.shadowmapIdx == ~0u)
                return;

            Camera shadowCam{};
            shadowCam.position = t.position;
            shadowCam.rotation = t.rotation;
            shadowCam.verticalFOV = worldLight.spotOuterCutoff * 2.0f;
            shadowCam.near = worldLight.shadowNear;
            shadowCam.far = worldLight.shadowFar;

            glm::mat4 vp = shadowCam.getProjectMatrixNonInfinite(1.0f) * shadowCam.getViewMatrix();
            Frustum f{};
            f.fromVPMatrix(vp);

            VK::RenderPass rp{};
            rp.RenderArea(SHADOWMAP_RES, SHADOWMAP_RES);
            rp.DepthAttachment(
                shadowmapInfo[worldLight.shadowmapIdx].texture.Get(), VK::LoadOp::Clear, VK::StoreOp::Store);
            rp.DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f));

            rp.Begin(cb);

            registry.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& woT) {
                const RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

                if (!cullMesh(rmi, woT, &f, 1))
                    return;

                glm::mat4 mvp = vp * woT.getMatrix();
                cb.PushConstants(mvp, VK::ShaderStage::Vertex, pipelineLayout.Get());

                for (int i = 0; i < rmi.numSubmeshes; i++)
                {
                    if (!wo.drawSubmeshes[i]) continue;
                    const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                    cb.DrawIndexed(
                        rsi.indexCount,
                        1,
                        rsi.indexOffset + (rmi.indexOffset / sizeof(uint32_t)),
                        rmi.vertsOffset / sizeof(Vertex),
                        0);
                }
            });

            rp.End(cb);
            shadowmapInfo[worldLight.shadowmapIdx].texture->Acquire(
                cb,
                VK::ImageLayout::ShaderReadOnlyOptimal,
                VK::AccessFlags::ShaderSampledRead,
                VK::PipelineStageFlags::FragmentShader);
        });

        cb.EndDebugLabel();
    }

    uint32_t ShadowmapManager::GetShadowmapId(uint32_t idx)
    {
        return shadowmapInfo[idx].bindlessID;
    }
}