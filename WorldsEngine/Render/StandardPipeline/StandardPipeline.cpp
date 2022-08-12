#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VK.hpp>
#include <Render/Frustum.hpp>
#include <Render/MaterialManager.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/StandardPipeline/Bloom.hpp>
#include <Render/StandardPipeline/CubemapConvoluter.hpp>
#include <Render/StandardPipeline/DebugLineDrawer.hpp>
#include <Render/StandardPipeline/LightCull.hpp>
#include <Render/StandardPipeline/Tonemapper.hpp>
#include <entt/entity/registry.hpp>
#include <Util/JsonUtil.hpp>
#include <Util/AABB.hpp>

#include <deque>

using namespace R2;

namespace worlds
{
    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
        uint32_t materialID;
        glm::vec2 textureScale;
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

    struct LightTile
    {
        uint32_t lightIdMasks[8];
        uint32_t cubemapIdMasks[2];
        uint32_t aoBoxIdMasks[2];
        uint32_t aoSphereIdMasks[2];
    };

    const VK::TextureFormat colorBufferFormat = VK::TextureFormat::B10G11R11_UFLOAT_PACK32;

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
    }

    StandardPipeline::~StandardPipeline()
    {
    }

    void StandardPipeline::setup(VKRTTPass* rttPass)
    {
        VK::Core* core = renderer->getCore();
        this->rttPass = rttPass;
        
        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffers[0] = core->CreateBuffer(modelMatrixBci);
        modelMatrixBuffers[1] = core->CreateBuffer(modelMatrixBci);

        if (materialBuffer == nullptr)
        {
            VK::BufferCreateInfo materialBci{ VK::BufferUsage::Storage, sizeof(uint32_t) * 4 * 128, true };
            materialBuffer = new SubAllocatedBuffer(core, materialBci);
        }

        VK::BufferCreateInfo lightBci{ VK::BufferUsage::Storage, sizeof(LightUB), true };
        lightBuffer = core->CreateBuffer(lightBci);

        VK::BufferCreateInfo lightTileBci{ VK::BufferUsage::Storage, sizeof(LightTile) * ((rttPass->width + 31) / 32) * ((rttPass->height + 31) / 32), true };
        lightTileBuffer = core->CreateBuffer(lightTileBci);

        AssetID vs = AssetDB::pathToId("Shaders/standard.vert.spv");
        AssetID fs = AssetDB::pathToId("Shaders/standard.frag.spv");
        AssetID depthFS = AssetDB::pathToId("Shaders/standard_empty.frag.spv");

        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(1, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(2, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(3, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(4, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.UpdateAfterBind();
        descriptorSetLayout = dslb.Build();

        for (int i = 0; i < 2; i++)
        {
            descriptorSets[i] = core->CreateDescriptorSet(descriptorSetLayout.Get());

            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, multiVPBuffer.Get());
            dsu.AddBuffer(1, 0, VK::DescriptorType::StorageBuffer, modelMatrixBuffers[i].Get());
            dsu.AddBuffer(2, 0, VK::DescriptorType::StorageBuffer, materialBuffer->GetBuffer());
            dsu.AddBuffer(3, 0, VK::DescriptorType::StorageBuffer, lightBuffer.Get());
            dsu.AddBuffer(4, 0, VK::DescriptorType::StorageBuffer, lightTileBuffer.Get());
            dsu.Update();
        }

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(StandardPushConstants))
            .DescriptorSet(descriptorSetLayout.Get())
            .DescriptorSet(&renderer->getBindlessTextureManager()->GetTextureDescriptorSetLayout());
        pipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
        vb.Attributes.emplace_back(1, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, normal));
        vb.Attributes.emplace_back(2, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, tangent));
        vb.Attributes.emplace_back(3, VK::TextureFormat::R32_SFLOAT, (uint32_t)offsetof(Vertex, bitangentSign));
        vb.Attributes.emplace_back(4, VK::TextureFormat::R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));

        VK::ShaderModule& stdVert = ShaderCache::getModule(vs);
        VK::ShaderModule& stdFrag = ShaderCache::getModule(fs);
        VK::ShaderModule& depthFrag = ShaderCache::getModule(depthFS);

        VK::PipelineBuilder pb{core};
        pb.PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout.Get())
            .ColorAttachmentFormat(colorBufferFormat)
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, stdFrag)
            .DepthTest(true)
            .DepthWrite(false)
            .DepthCompareOp(VK::CompareOp::Equal)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .MSAASamples(rttPass->getSettings().msaaLevel);

        pipeline = pb.Build();

        VK::PipelineBuilder pb2{core};
        pb2.PrimitiveTopology(VK::Topology::TriangleList)
            .CullMode(VK::CullMode::Back)
            .Layout(pipelineLayout.Get())
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, stdVert)
            .AddShader(VK::ShaderStage::Fragment, depthFrag)
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .MSAASamples(rttPass->getSettings().msaaLevel);

        depthPrePipeline = pb2.Build();

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);

        depthBufferCI.Samples = rttPass->getSettings().msaaLevel;
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);

        colorBufferCI.Samples = rttPass->getSettings().msaaLevel;
        colorBuffer = core->CreateTexture(colorBufferCI);

        cubemapConvoluter = new CubemapConvoluter(core);

        lightCull = new LightCull(core, depthBuffer.Get(), lightBuffer.Get(), lightTileBuffer.Get(), multiVPBuffer.Get());
        debugLineDrawer = new DebugLineDrawer(core, multiVPBuffer.Get(), rttPass->getSettings().msaaLevel);
        bloom = new Bloom(core, colorBuffer.Get());
        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget(), bloom->GetOutput());
    }

    void StandardPipeline::onResize(int width, int height)
    {
        VK::Core* core = renderer->getCore();
        depthBuffer.Reset();
        colorBuffer.Reset();

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);

        depthBufferCI.Samples = rttPass->getSettings().msaaLevel;
        depthBuffer = core->CreateTexture(depthBufferCI);

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);

        colorBufferCI.Samples = rttPass->getSettings().msaaLevel;
        colorBuffer = core->CreateTexture(colorBufferCI);

        VK::BufferCreateInfo lightTileBci{ VK::BufferUsage::Storage, sizeof(LightTile) * ((rttPass->width + 31) / 32) * ((rttPass->height + 31) / 32), true };
        lightTileBuffer = core->CreateBuffer(lightTileBci);

        for (int i = 0; i < 2; i++)
        {
            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            dsu.AddBuffer(4, 0, VK::DescriptorType::StorageBuffer, lightTileBuffer.Get());
            dsu.Update();
        }

        lightCull = new LightCull(core, depthBuffer.Get(), lightBuffer.Get(), lightTileBuffer.Get(), multiVPBuffer.Get());
        debugLineDrawer = new DebugLineDrawer(core, multiVPBuffer.Get(), rttPass->getSettings().msaaLevel);
        bloom = new Bloom(core, colorBuffer.Get());
        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget(), bloom->GetOutput());
    }

    glm::vec3 toLinear(glm::vec3 sRGB)
    {
        glm::bvec3 cutoff = glm::lessThan(sRGB, glm::vec3(0.04045f));
        glm::vec3 higher = glm::pow((sRGB + glm::vec3(0.055f)) / glm::vec3(1.055f), glm::vec3(2.4f));
        glm::vec3 lower = sRGB / 12.92f;

        return mix(higher, lower, cutoff);
    }

    void StandardPipeline::drawLoop(entt::registry& reg, R2::VK::CommandBuffer& cb, bool writeMatrices, Frustum& frustum)
    {
        RenderMeshManager* meshManager = renderer->getMeshManager();
        VK::Core* core = renderer->getCore();

        uint32_t modelMatrixIndex = 0;
        glm::mat4* modelMatricesMapped;

        if (writeMatrices)
            modelMatricesMapped = (glm::mat4*)modelMatrixBuffers[core->GetFrameIndex()]->Map();

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t)
        {
            const RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

            float maxScale = glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
            if (!frustum.containsSphere(t.position, maxScale * rmi.boundingSphereRadius)) return;

            AABB aabb = AABB{rmi.aabbMin, rmi.aabbMax}.transform(t);
            if (!frustum.containsAABB(aabb.min, aabb.max)) return;

            if (writeMatrices)
                modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();
            else
                modelMatrixIndex++;

            cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), rmi.vertsOffset);

            VK::IndexType vkIdxType =
                rmi.indexType == IndexType::Uint32 ? VK::IndexType::Uint32 : VK::IndexType::Uint16;
            cb.BindIndexBuffer(meshManager->getIndexBuffer(), rmi.indexOffset, vkIdxType);

            for (int i = 0; i < rmi.numSubmeshes; i++)
            {
                const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];

                StandardPushConstants spc{};
                spc.modelMatrixID = modelMatrixIndex - 1;
                spc.textureScale = glm::vec2(wo.texScaleOffset.x, wo.texScaleOffset.y);
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

            if (writeMatrices)
                modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();
            else
                modelMatrixIndex++;

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

                spc.materialID = loadOrGetMaterial(renderer, wo.materials[materialIndex]);
                spc.textureScale = glm::vec2(wo.texScaleOffset.x, wo.texScaleOffset.y);
                spc.textureOffset = glm::vec2(wo.texScaleOffset.z, wo.texScaleOffset.w);
                cb.PushConstants(spc, VK::ShaderStage::AllRaster, pipelineLayout.Get());

                cb.DrawIndexed(rsi.indexCount, 1, rsi.indexOffset, 0, 0);
            }
        });

        if (writeMatrices)
            modelMatrixBuffers[core->GetFrameIndex()]->Unmap();
    }

    std::deque<AssetID> convoluteQueue;

    void StandardPipeline::fillLightBuffer(entt::registry& reg, VKTextureManager* textureManager)
    {
        LightUB* lMapped = (LightUB*)lightBuffer->Map();

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
            pl.setOuterCutoff(wl.spotOuterCutoff);
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

            lMapped->lights[lightCount] = pl;
            lightCount++;
        });

        lMapped->lightCount = lightCount;

        uint32_t cubemapIdx = 1;
        lMapped->cubemaps[0] = GPUCubemap { glm::vec3{100000.0f}, ~0u, glm::vec3{0.0f}, 0 };
        reg.view<WorldCubemap, Transform>().each([&](WorldCubemap& wc, Transform& t)
        {
            GPUCubemap gc{};
            gc.extent = wc.extent;
            gc.position = t.position;
            gc.flags = wc.cubeParallax;
            if (!textureManager->isLoaded(wc.cubemapId))
            {
                convoluteQueue.push_back(wc.cubemapId);
            }
            gc.texture = textureManager->loadOrGet(wc.cubemapId);
            lMapped->cubemaps[cubemapIdx] = gc;
            wc.renderIdx = cubemapIdx;
            cubemapIdx++;
        });

        lMapped->cubemapCount = cubemapIdx;

        lightBuffer->Unmap();
    }

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        MaterialManager* materialManager = renderer->getMaterialManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();

        fillLightBuffer(reg, textureManager);

        if (convoluteQueue.size() > 0)
        {
            cb.BeginDebugLabel("Cubemap Convolution", 0.1f, 0.1f, 0.1f);

            AssetID convoluteID = convoluteQueue.front();
            convoluteQueue.pop_front();
            uint32_t convoluteHandle = textureManager->loadOrGet(convoluteID);
            VK::Texture* tex = btm->GetTextureAt(convoluteHandle);
            cubemapConvoluter->Convolute(cb, tex);

            cb.EndDebugLabel();
        }

        modelMatrixBuffers[core->GetFrameIndex()]->Acquire(cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::VertexShader);
        cb.BeginDebugLabel("Depth Pre-Pass", 0.1f, 0.1f, 0.1f);
        cb.BindPipeline(depthPrePipeline.Get());
        VK::RenderPass depthPass;
        depthPass
            .DepthAttachment(depthBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f))
            .RenderArea(rttPass->width, rttPass->height)
            .Begin(cb);

        Camera* camera = rttPass->getCamera();

        MultiVP multiVPs{};
        multiVPs.screenWidth = rttPass->width;
        multiVPs.screenHeight = rttPass->height;
        multiVPs.views[0] = camera->getViewMatrix();
        multiVPs.projections[0] = camera->getProjectionMatrix((float)rttPass->width / rttPass->height);
        multiVPs.inverseVP[0] =  glm::inverse(multiVPs.projections[0] * multiVPs.views[0]);
        multiVPs.viewPos[0] = glm::vec4(camera->position, 0.0f);

        Frustum frustum{};
        frustum.fromVPMatrix(multiVPs.projections[0] * multiVPs.views[0]);

        core->QueueBufferUpload(multiVPBuffer.Get(), &multiVPs, sizeof(multiVPs), 0);

        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), descriptorSets[core->GetFrameIndex()]->GetNativeHandle(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), btm->GetTextureDescriptorSet().GetNativeHandle(), 1);
        cb.SetViewport(VK::Viewport::Simple((float)rttPass->width, (float)rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));

        drawLoop(reg, cb, true, frustum);
        depthPass.End(cb);

        cb.EndDebugLabel();

        lightCull->Execute(cb);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::FragmentShader);
        
        cb.BeginDebugLabel("Opaque Pass", 0.5f, 0.1f, 0.1f);
        cb.BindPipeline(pipeline.Get());
        VK::RenderPass colorPass;
        colorPass.ColorAttachment(colorBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(glm::pow(0.392f, 2.2f), glm::pow(0.584f, 2.2f), glm::pow(0.929f, 2.2f), 1.0f))
            .DepthAttachment(depthBuffer.Get(), VK::LoadOp::Load, VK::StoreOp::Store)
            .RenderArea(rttPass->width, rttPass->height)
            .Begin(cb);

        drawLoop(reg, cb, false, frustum);

        size_t dbgLinesCount;
        const DebugLine* dbgLines = renderer->getCurrentDebugLines(&dbgLinesCount);

        cb.BeginDebugLabel("Debug Lines", 0.1f, 0.1f, 0.1f);
        debugLineDrawer->Execute(cb, dbgLines, dbgLinesCount);
        cb.EndDebugLabel();

        colorPass.End(cb);
        cb.EndDebugLabel();

        bloom->Execute(cb);

        tonemapper->Execute(cb);
    }
}