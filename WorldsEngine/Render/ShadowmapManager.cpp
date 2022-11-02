#include <Render/RenderInternal.hpp>
#include <Core/AssetDB.hpp>
#include <Core/ConVar.hpp>
#include <entt/entity/registry.hpp>
#include <Render/CullMesh.hpp>
#include <Render/Frustum.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VK.hpp>

using namespace R2;

namespace worlds
{
    ConVar r_skipShadows {"r_skipShadows", "0"};
    ConVar r_shadowmapRes {"r_shadowmapRes", "512"};
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
            VK::TextureCreateInfo tci = VK::TextureCreateInfo::RenderTarget2D(
                VK::TextureFormat::D32_SFLOAT,
                r_shadowmapRes.getInt(), r_shadowmapRes.getInt()
            );
            ShadowmapInfo si{};
            si.texture = renderer->getCore()->CreateTexture(tci);
            si.bindlessID = renderer->getBindlessTextureManager()->AllocateTextureHandle(si.texture.Get());
            shadowmapInfo.push_back(std::move(si));
        }
        shadowmapMatrices.resize(NUM_SHADOW_LIGHTS);
    }

    void ShadowmapManager::AllocateShadowmaps(entt::registry& registry)
    {
        uint32_t count = 0;
        registry.view<WorldLight>().each([&](WorldLight& worldLight) {
            bool isShadowable = worldLight.type == LightType::Spot || worldLight.type == LightType::Directional;
            if (!worldLight.enableShadows || !isShadowable || count == NUM_SHADOW_LIGHTS)
            {
                worldLight.shadowmapIdx = ~0u;
                return;
            }

            worldLight.shadowmapIdx = count++;
        });
    }

    glm::mat4 getCascadeMatrix(glm::mat4 camVP, glm::vec3 lightDir, float& texelsPerUnit)
    {
        glm::mat4 vpInv = glm::inverse(camVP);

        glm::vec3 frustumCorners[8] = {
                glm::vec3(-1.0f,  1.0f, -1.0f),
                glm::vec3(1.0f,  1.0f, -1.0f),
                glm::vec3(1.0f, -1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3(1.0f,  1.0f,  1.0f),
                glm::vec3(1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        for (int i = 0; i < 8; i++)
        {
            glm::vec4 transformed = vpInv * glm::vec4{ frustumCorners[i], 1.0f };
            transformed /= transformed.w;
            frustumCorners[i] = transformed;
        }

        glm::vec3 center{ 0.0f };

        for (int i = 0; i < 8; i++)
        {
            center += frustumCorners[i];
        }

        center /= 8.0f;

        float diameter = 0.0f;
        for (int i = 0; i < 8; i++)
        {
            float dist = glm::length(frustumCorners[i] - center);
            diameter = glm::max(diameter, dist);
        }
        float radius = diameter * 0.5f;

        texelsPerUnit = r_shadowmapRes.getFloat() / diameter;

        glm::mat4 scaleMatrix = glm::scale(glm::mat4{ 1.0f }, glm::vec3{ texelsPerUnit });

        glm::mat4 lookAt = glm::lookAt(glm::vec3{ 0.0f }, lightDir, glm::vec3{ 0.0f, 1.0f, 0.0f });
        lookAt *= scaleMatrix;

        glm::mat4 lookAtInv = glm::inverse(lookAt);

        center = lookAt * glm::vec4{ center, 1.0f };
        center = glm::floor(center);
        center = lookAtInv * glm::vec4{ center, 1.0f };

        glm::vec3 eye = center + (lightDir * diameter);

        glm::mat4 viewMat = glm::lookAt(eye, center, glm::vec3{ 0.0f, 1.0f, 0.0f });
        glm::mat4 projMat = glm::orthoZO(-radius, radius, -radius, radius, radius * 20.0f, -radius * 20.0f);

        return projMat * viewMat;
    }

    void ShadowmapManager::RenderShadowmaps(R2::VK::CommandBuffer& cb, entt::registry& registry, glm::mat4& viewMatrix)
    {
        RenderMeshManager* meshManager = renderer->getMeshManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();

        if (r_skipShadows) return;

        cb.BeginDebugLabel("Shadows", 0.1f, 0.1f, 0.1f);
        int shadowRes = r_shadowmapRes.getInt();

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++)
        {
            ShadowmapInfo& info = shadowmapInfo[i];

            if (info.texture->GetWidth() != shadowRes)
            {
                VK::TextureCreateInfo tci =
                    VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, shadowRes, shadowRes);
                info.texture = renderer->getCore()->CreateTexture(tci);
                btm->SetTextureAt(info.bindlessID, info.texture.Get());
            }
        }

        cb.BindPipeline(pipeline.Get());
        cb.SetScissor(VK::ScissorRect::Simple(shadowRes, shadowRes));
        cb.SetViewport(VK::Viewport::Simple(shadowRes, shadowRes));
        cb.BindIndexBuffer(meshManager->getIndexBuffer(), 0, VK::IndexType::Uint32);
        cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), 0);

        registry.view<WorldLight, Transform>().each([&](WorldLight& worldLight, Transform& t) {
            bool isShadowable = worldLight.type == LightType::Spot || worldLight.type == LightType::Directional;
            if (!worldLight.enableShadows || !isShadowable || worldLight.shadowmapIdx == ~0u)
                return;

            glm::mat4 vp;

            if (worldLight.type == LightType::Spot)
            {
                Camera shadowCam{};
                shadowCam.position = t.position;
                shadowCam.rotation = t.rotation;
                shadowCam.verticalFOV = worldLight.spotOuterCutoff * 2.0f;
                shadowCam.near = worldLight.shadowNear;
                shadowCam.far = worldLight.shadowFar;

                vp = shadowCam.getProjectMatrixNonInfinite(1.0f) * shadowCam.getViewMatrix();
            }
            else
            {
                Camera viewerCopy{};
                viewerCopy.near = 0.1f;
                viewerCopy.far = 50.0f;
                viewerCopy.verticalFOV = 1.8f;
                glm::mat4 cascadeVp = viewerCopy.getProjectionMatrixZONonInfinite(1.0f) * viewMatrix;
                float whatever;
                vp = getCascadeMatrix(cascadeVp, t.transformDirection(glm::vec3(0.0f, 0.0f, -1.0f)),
                                      whatever);
            }

            shadowmapMatrices[worldLight.shadowmapIdx] = vp;

            Frustum f{};
            f.fromVPMatrix(vp);

            VK::RenderPass rp{};
            rp.RenderArea(shadowRes, shadowRes);
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

            registry.view<SkinnedWorldObject, Transform>().each([&](SkinnedWorldObject& wo, Transform& woT) {
                const RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

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
                        wo.skinnedVertexOffset + (meshManager->getSkinnedVertsOffset() / sizeof(Vertex)),
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

    glm::mat4& ShadowmapManager::GetShadowVPMatrix(uint32_t idx)
    {
        return shadowmapMatrices[idx];
    }
}
