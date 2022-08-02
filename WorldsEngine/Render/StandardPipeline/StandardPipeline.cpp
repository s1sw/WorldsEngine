#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/SubAllocatedBuffer.hpp>
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
#include <Render/StandardPipeline/Tonemapper.hpp>
#include <entt/entity/registry.hpp>
#include <Util/JsonUtil.hpp>

using namespace R2;

namespace worlds
{
    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
        uint32_t materialID;
        uint32_t lightCount;
        uint32_t pad;
        glm::vec2 textureOffset;
    };

    struct StandardPBRMaterial
    {
        uint32_t albedoTexture;
        uint32_t normalTexture;
        uint32_t mraTexture;
        float defaultRoughness;
        float defaultMetallic;
        glm::vec3 emissiveColor;
    };

    const VK::TextureFormat colorBufferFormat = VK::TextureFormat::R16G16B16A16_SFLOAT;

    struct MaterialAllocInfo
    {
        size_t offset;
        SubAllocationHandle handle;
    };

    robin_hood::unordered_map<AssetID, MaterialAllocInfo> allocedMaterials;
    SubAllocatedBuffer* materialBuffer = nullptr;

    size_t loadOrGetMaterial(VKRenderer* renderer, AssetID id)
    {
        if (allocedMaterials.contains(id))
        {
            return allocedMaterials[id].offset;
        }

        MaterialManager* materialManager = renderer->getMaterialManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* tm = renderer->getTextureManager();

        MaterialAllocInfo mai{};

        auto& j = materialManager->loadOrGet(id);

        mai.offset = materialBuffer->Allocate(sizeof(StandardPBRMaterial), mai.handle);

        StandardPBRMaterial material{};
        material.albedoTexture = tm->loadOrGet(AssetDB::pathToId(j["albedoPath"]));
        material.normalTexture = ~0u;
        material.mraTexture = ~0u;

        if (j.contains("normalMapPath"))
        {
            material.normalTexture = tm->loadOrGet(AssetDB::pathToId(j["normalMapPath"]));
        }

        if (j.contains("pbrMapPath"))
        {
            material.mraTexture = tm->loadOrGet(AssetDB::pathToId(j["pbrMapPath"]));
        }

        material.defaultMetallic = j.value("metallic", 0.0f);
        material.defaultRoughness = j.value("roughness", 0.5f);
        if (j.contains("emissiveColor"))
            material.emissiveColor = j["emissiveColor"].get<glm::vec3>();
        else
            material.emissiveColor = glm::vec3(0.0f);

        renderer->getCore()->QueueBufferUpload(materialBuffer->GetBuffer(), &material, sizeof(material), mai.offset);

        allocedMaterials.insert({ id, mai });

        return mai.offset;
    }

    StandardPipeline::StandardPipeline(VKRenderer* renderer) : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();
        
        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffer = core->CreateBuffer(modelMatrixBci);

        if (materialBuffer == nullptr)
        {
            VK::BufferCreateInfo materialBci{ VK::BufferUsage::Storage, sizeof(uint32_t) * 4 * 64, true };
            materialBuffer = new SubAllocatedBuffer(core, materialBci);
        }

        VK::BufferCreateInfo lightBci{ VK::BufferUsage::Storage, sizeof(PackedLight) * 64, true };
        lightBuffer = core->CreateBuffer(lightBci);

        AssetID vs = AssetDB::pathToId("Shaders/standard.vert.spv");
        AssetID fs = AssetDB::pathToId("Shaders/standard.frag.spv");

        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(1, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(2, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(3, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        descriptorSetLayout = dslb.Build();

        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout.Get());

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, multiVPBuffer.Get());
        dsu.AddBuffer(1, 0, VK::DescriptorType::StorageBuffer, modelMatrixBuffer.Get());
        dsu.AddBuffer(2, 0, VK::DescriptorType::StorageBuffer, materialBuffer->GetBuffer());
        dsu.AddBuffer(3, 0, VK::DescriptorType::StorageBuffer, lightBuffer.Get());
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(descriptorSetLayout.Get())
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

        VK::PipelineBuilder pb{core};
        pb.PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout.Get())
            .ColorAttachmentFormat(colorBufferFormat)
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
    }

    void StandardPipeline::setup(VKRTTPass* rttPass)
    {
        VK::Core* core = renderer->getCore();
        this->rttPass = rttPass;

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);
        colorBuffer = core->CreateTexture(colorBufferCI);

        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget());
    }

    void StandardPipeline::onResize(int width, int height)
    {
        VK::Core* core = renderer->getCore();
        depthBuffer.Reset();
        colorBuffer.Reset();

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);
        colorBuffer = core->CreateTexture(colorBufferCI);

        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget());
    }

    glm::vec3 toLinear(glm::vec3 sRGB)
    {
        glm::bvec3 cutoff = glm::lessThan(sRGB, glm::vec3(0.04045f));
        glm::vec3 higher = glm::pow((sRGB + glm::vec3(0.055f)) / glm::vec3(1.055f), glm::vec3(2.4f));
        glm::vec3 lower = sRGB / 12.92f;

        return mix(higher, lower, cutoff);
    }

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        MaterialManager* materialManager = renderer->getMaterialManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();

        PackedLight* lMapped = (PackedLight*)lightBuffer->Map();

        uint32_t lightCount = 0;
        reg.view<WorldLight, Transform>().each([&](WorldLight& wl, const Transform& t)
        {
            if (!wl.enabled) return;
            if (wl.type == LightType::Sphere) return;

            glm::vec3 lightForward = glm::normalize(t.transformDirection(glm::vec3(0.0f, 0.0f, -1.0f)));

            PackedLight pl;
            pl.color = toLinear(wl.color) * wl.intensity;
            pl.setLightType(wl.type);
            pl.distanceCutoff = wl.maxDistance;
            pl.direction = lightForward;
            pl.spotCutoff = glm::cos(wl.spotCutoff);
            pl.position = t.position;

            // Tube lights are packed in a weird way
            if (wl.type == LightType::Tube)
            {
                glm::vec3 tubeP0 = t.position + lightForward * wl.tubeLength;
                glm::vec3 tubeP1 = t.position - lightForward * wl.tubeLength;

                pl.direction = tubeP0;
                pl.spotCutoff = wl.tubeRadius;
                pl.position = tubeP1;
            }

            lMapped[lightCount] = pl;
            lightCount++;
        });

        lightBuffer->Unmap();

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrixBuffer->Map();

        VK::RenderPass r;
        r.ColorAttachment(colorBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(1.f, 0.f, 1.f, 1.f))
            .DepthAttachment(depthBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f))
            .RenderArea(rttPass->width, rttPass->height)
            .Begin(cb);

        Camera* camera = rttPass->getCamera();

        MultiVP multiVPs{};
        multiVPs.views[0] = camera->getViewMatrix();
        multiVPs.projections[0] = camera->getProjectionMatrix((float)rttPass->width / rttPass->height);
        multiVPs.viewPos[0] = glm::vec4(camera->position, 0.0f);

        core->QueueBufferUpload(multiVPBuffer.Get(), &multiVPs, sizeof(multiVPs), 0);

        uint32_t modelMatrixIndex = 0;

        cb.BindPipeline(pipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), descriptorSet->GetNativeHandle(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), btm->GetTextureDescriptorSet().GetNativeHandle(), 1);
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
                spc.lightCount = lightCount;
                spc.textureOffset = glm::vec2(wo.texScaleOffset.z, wo.texScaleOffset.w);
                uint8_t materialIndex = rsi.materialIndex;

                if (!wo.presentMaterials[materialIndex])
                {
                    materialIndex = 0;
                }

                spc.materialID = loadOrGetMaterial(renderer, wo.materials[materialIndex]);
                cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout.Get());

                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });

        reg.view<SkinnedWorldObject, Transform>().each([&](SkinnedWorldObject& wo, Transform& t)
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
                spc.lightCount = lightCount;
                uint8_t materialIndex = rsi.materialIndex;

                if (!wo.presentMaterials[materialIndex])
                {
                    materialIndex = 0;
                }

                spc.materialID = loadOrGetMaterial(renderer, wo.materials[materialIndex]);
                cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout.Get());

                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });

        r.End(cb);

        modelMatrixBuffer->Unmap();

        tonemapper->Execute(cb);
    }
}