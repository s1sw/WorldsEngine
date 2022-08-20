#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <Core/Engine.hpp>
#include <Core/MaterialManager.hpp>
#include <Core/TaskScheduler.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VK.hpp>
#include <Render/Frustum.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/StandardPipeline/Bloom.hpp>
#include <Render/StandardPipeline/CubemapConvoluter.hpp>
#include <Render/StandardPipeline/DebugLineDrawer.hpp>
#include <Render/StandardPipeline/LightCull.hpp>
#include <Render/StandardPipeline/SkyboxRenderer.hpp>
#include <Render/StandardPipeline/Tonemapper.hpp>
#include <entt/entity/registry.hpp>
#include <Util/JsonUtil.hpp>
#include <Util/AABB.hpp>
#include <Tracy.hpp>

#include <deque>
#include <new>

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

    std::mutex allocedMaterialsLock;
    robin_hood::unordered_map<AssetID, MaterialAllocInfo> allocedMaterials;
    SubAllocatedBuffer* materialBuffer = nullptr;

    uint32_t getViewMask(int viewCount)
    {
        if (viewCount == 1)
            return 0;

        uint32_t mask = 0;
        for (int i = 0; i < viewCount; i++)
        {
            mask |= 1 << i;
        }

        return mask;
    }

    size_t loadOrGetMaterial(VKRenderer* renderer, AssetID id)
    {
        MaterialAllocInfo mai{};
        {
            std::unique_lock lock{allocedMaterialsLock};

            if (allocedMaterials.contains(id))
            {
                return allocedMaterials[id].offset;
            }

            mai.offset = materialBuffer->Allocate(sizeof(StandardPBRMaterial), mai.handle);
            allocedMaterials.insert({id, mai});
        }

        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* tm = renderer->getTextureManager();


        auto& j = MaterialManager::loadOrGet(id);

        StandardPBRMaterial material{};
        material.albedoTexture = tm->loadOrGet(AssetDB::pathToId(j.value("albedoPath", "Textures/missing.wtex")));
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


        return mai.offset;
    }

    StandardPipeline::StandardPipeline(VKRenderer* renderer) : renderer(renderer)
    {
    }

    StandardPipeline::~StandardPipeline()
    {
    }

    void StandardPipeline::createSizeDependants()
    {
        ZoneScoped;

        const RTTPassSettings& settings = rttPass->getSettings();
        VK::Core* core = renderer->getCore();
        depthBuffer.Reset();
        colorBuffer.Reset();

        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);

        depthBufferCI.Samples = rttPass->getSettings().msaaLevel;
        depthBufferCI.Layers = settings.numViews;
        depthBuffer = core->CreateTexture(depthBufferCI);
        depthBuffer->SetDebugName("Depth Buffer");

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);

        colorBufferCI.Samples = rttPass->getSettings().msaaLevel;
        colorBufferCI.Layers = settings.numViews;
        colorBuffer = core->CreateTexture(colorBufferCI);
        colorBuffer->SetDebugName("Color Buffer");

        VK::BufferCreateInfo lightTileBci{
            VK::BufferUsage::Storage,
            sizeof(LightTile) * ((rttPass->width + 31) / 32) * ((rttPass->height + 31) / 32) * settings.numViews, true};
        lightTileBuffer = core->CreateBuffer(lightTileBci);
        lightTileBuffer->SetDebugName("Light Tile Buffer");

        for (int i = 0; i < 2; i++)
        {
            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            dsu.AddBuffer(4, 0, VK::DescriptorType::StorageBuffer, lightTileBuffer.Get());
            dsu.Update();
        }

        lightCull =
            new LightCull(core, depthBuffer.Get(), lightBuffer.Get(), lightTileBuffer.Get(), multiVPBuffer.Get());
        debugLineDrawer = new DebugLineDrawer(core, multiVPBuffer.Get(), rttPass->getSettings().msaaLevel,
                                              getViewMask(settings.numViews));
        bloom = new Bloom(core, colorBuffer.Get());
        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget(), bloom->GetOutput());
        skyboxRenderer = new SkyboxRenderer(core, pipelineLayout.Get(), settings.msaaLevel, getViewMask(settings.numViews));
    }

    void StandardPipeline::setup(VKRTTPass* rttPass)
    {
        ZoneScoped;
        const RTTPassSettings& settings = rttPass->getSettings();
        VK::Core* core = renderer->getCore();
        this->rttPass = rttPass;

        if (rttPass->getSettings().numViews > 1)
        {
            useViewOverrides = true;
            overrideViews.resize(rttPass->getSettings().numViews);
            overrideProjs.resize(rttPass->getSettings().numViews);
        }

        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);
        multiVPBuffer->SetDebugName("View Info Buffer");

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffers[0] = core->CreateBuffer(modelMatrixBci);
        modelMatrixBuffers[1] = core->CreateBuffer(modelMatrixBci);

        if (materialBuffer == nullptr)
        {
            VK::BufferCreateInfo materialBci{VK::BufferUsage::Storage, sizeof(uint32_t) * 8 * 128, true};
            materialBuffer = new SubAllocatedBuffer(core, materialBci);
            materialBuffer->GetBuffer()->SetDebugName("Material Buffer");
        }

        VK::BufferCreateInfo lightBci{VK::BufferUsage::Storage, sizeof(LightUB), true};
        lightBuffer = core->CreateBuffer(lightBci);
        lightBuffer->SetDebugName("Light Buffer");

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

        pb.ViewMask(getViewMask(settings.numViews));

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

        pb2.ViewMask(getViewMask(settings.numViews));

        depthPrePipeline = pb2.Build();

        cubemapConvoluter = new CubemapConvoluter(core);
        createSizeDependants();
    }

    void StandardPipeline::onResize(int width, int height)
    {
        createSizeDependants();
    }

    glm::vec3 toLinear(glm::vec3 sRGB)
    {
        glm::bvec3 cutoff = glm::lessThan(sRGB, glm::vec3(0.04045f));
        glm::vec3 higher = glm::pow((sRGB + glm::vec3(0.055f)) / glm::vec3(1.055f), glm::vec3(2.4f));
        glm::vec3 lower = sRGB / 12.92f;

        return mix(higher, lower, cutoff);
    }

    bool cullMesh(const RenderMeshInfo& rmi, const Transform& t, Frustum* frustums, int numViews)
    {
        float maxScale = glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
        bool reject = true;
        for (int i = 0; i < numViews; i++)
        {
            if (frustums[i].containsSphere(t.position, maxScale * rmi.boundingSphereRadius))
                reject = false;
        }

        if (reject)
            return false;

        reject = false;
        AABB aabb = AABB{rmi.aabbMin, rmi.aabbMax}.transform(t);
        for (int i = 0; i < numViews; i++)
        {
            if (frustums[i].containsAABB(aabb.min, aabb.max))
                reject = false;
        }

        if (reject)
            return false;

        return true;
    }

    void StandardPipeline::drawLoop(entt::registry& reg, R2::VK::CommandBuffer& cb, bool writeMatrices,
                                    Frustum* frustums, int numViews)
    {
        ZoneScoped;
        RenderMeshManager* meshManager = renderer->getMeshManager();
        VK::Core* core = renderer->getCore();

        uint32_t modelMatrixIndex = 0;
        glm::mat4* modelMatricesMapped;

        if (writeMatrices)
            modelMatricesMapped = (glm::mat4*)modelMatrixBuffers[core->GetFrameIndex()]->Map();

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            const RenderMeshInfo& rmi = meshManager->loadOrGet(wo.mesh);

            if (!cullMesh(rmi, t, frustums, numViews))
                return;

            if (writeMatrices)
                modelMatricesMapped[modelMatrixIndex++] = t.getMatrix();
            else
                modelMatrixIndex++;


            VK::IndexType vkIdxType =
                rmi.indexType == IndexType::Uint32 ? VK::IndexType::Uint32 : VK::IndexType::Uint16;
            cb.BindIndexBuffer(meshManager->getIndexBuffer(), rmi.indexOffset, vkIdxType);
            cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), rmi.vertsOffset);

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

        reg.view<SkinnedWorldObject, Transform>().each([&](SkinnedWorldObject& wo, Transform& t) {
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
                spc.modelMatrixID = modelMatrixIndex - 1;
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
        ZoneScoped;
        LightUB* lMapped = (LightUB*)lightBuffer->Map();

        uint32_t lightCount = 0;
        reg.view<WorldLight, Transform>().each([&](WorldLight& wl, const Transform& t) {
            if (!wl.enabled)
                return;

            glm::vec3 lightForward = glm::normalize(t.transformDirection(glm::vec3(0.0f, 0.0f, -1.0f)));

            PackedLight pl;
            pl.color = toLinear(wl.color) * wl.intensity;
            pl.setLightType(wl.type);
            pl.distanceCutoff = wl.maxDistance;
            pl.direction = lightForward;

            // Sphere lights have their sphere radius written into the spotCutoff field
            // If it's a sphere light, we need to pass it through unmodified
            pl.spotCutoff = wl.type == LightType::Sphere ? wl.spotCutoff : glm::cos(wl.spotCutoff);

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

        AssetID skybox = reg.ctx<SceneSettings>().skybox;

        if (!textureManager->isLoaded(skybox))
        {
            convoluteQueue.push_back(skybox);
        }

        uint32_t cubemapIdx = 1;
        lMapped->cubemaps[0] = GPUCubemap{glm::vec3{100000.0f}, textureManager->loadOrGet(skybox), glm::vec3{0.0f}, 0};

        auto cubemapView = reg.view<WorldCubemap, Transform>();

        cubemapView.each([&](WorldCubemap& wc, Transform& t) {
            GPUCubemap gc{};
            gc.extent = wc.extent;
            gc.position = t.position;
            gc.flags = wc.cubeParallax;
            if (!textureManager->isLoaded(wc.cubemapId))
            {
                convoluteQueue.push_back(wc.cubemapId);
            }
            lMapped->cubemaps[cubemapIdx] = gc;
            wc.renderIdx = cubemapIdx;
            cubemapIdx++;
        });
        // gc.texture = textureManager->loadOrGet(wc.cubemapId);

        enki::TaskSet loadCubemapsTask(std::distance(cubemapView.begin(), cubemapView.end()), [&](enki::TaskSetPartition range, uint32_t) {
            auto begin = cubemapView.begin();
            auto end = cubemapView.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                WorldCubemap& wc = reg.get<WorldCubemap>(*it);
                lMapped->cubemaps[wc.renderIdx].texture = textureManager->loadOrGet(wc.cubemapId);
            }
        });

        g_taskSched.AddTaskSetToPipe(&loadCubemapsTask);
        g_taskSched.WaitforTask(&loadCubemapsTask);

        lMapped->cubemapCount = cubemapIdx;

        lightBuffer->Unmap();
    }

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        ZoneScoped;
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();

        // Fill the light buffer with lights + cubemaps from world
        // This will add cubemaps to the convolution queue if necessary
        fillLightBuffer(reg, textureManager);

        // If there's anything in the convolution queue, convolute 1 cubemap
        // per frame (convolution is slow!)
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

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderStorageRead, VK::PipelineStageFlags::FragmentShader);
        modelMatrixBuffers[core->GetFrameIndex()]->Acquire(cb, VK::AccessFlags::ShaderRead,
                                                           VK::PipelineStageFlags::VertexShader);

        // Set up culling frustums and fill the VP buffer
        Camera* camera = rttPass->getCamera();
        Frustum* frustums = (Frustum*)alloca(sizeof(Frustum) * rttPass->getSettings().numViews);

        MultiVP multiVPs{};
        multiVPs.screenWidth = rttPass->width;
        multiVPs.screenHeight = rttPass->height;
        for (int i = 0; i < rttPass->getSettings().numViews; i++)
        {
            glm::mat4 view;
            glm::mat4 proj;

            if (useViewOverrides)
            {
                view = overrideViews[i] * camera->getViewMatrix();
                proj = overrideProjs[i];
            }
            else
            {
                view = camera->getViewMatrix();
                proj = camera->getProjectionMatrix((float)rttPass->width / rttPass->height);
            }

            multiVPs.views[i] = view;
            multiVPs.projections[i] = proj;
            multiVPs.inverseVP[i] = glm::inverse(proj * view);
            multiVPs.viewPos[i] = glm::vec4((glm::vec3)glm::inverse(view)[3], 0.0f);

            new (&frustums[i]) Frustum();
            frustums[i].fromVPMatrix(proj * view);
        }

        core->QueueBufferUpload(multiVPBuffer.Get(), &multiVPs, sizeof(multiVPs), 0);

        // Taskified pre-caching of materials
        {
            auto worldObjectView = reg.view<WorldObject>();

            enki::TaskSet loadMatsTask(std::distance(worldObjectView.begin(), worldObjectView.end()), [&](enki::TaskSetPartition range, uint32_t) {
                auto begin = worldObjectView.begin();
                auto end = worldObjectView.begin();
                std::advance(begin, range.start);
                std::advance(end, range.end);

                for (auto it = begin; it != end; it++)
                {
                    WorldObject& wo = reg.get<WorldObject>(*it);

                    for (int i = 0; i < NUM_SUBMESH_MATS; i++)
                    {
                        if (!wo.presentMaterials[i]) continue;
                        loadOrGetMaterial(renderer, wo.materials[i]);
                    }
                }
            });

            g_taskSched.AddTaskSetToPipe(&loadMatsTask);
            g_taskSched.WaitforTask(&loadMatsTask);
        }

        // Do the same for skinned objects
        {
            auto skinnedView = reg.view<SkinnedWorldObject>();

            enki::TaskSet skinnedLoadMatsTask(std::distance(skinnedView.begin(), skinnedView.end()), [&](enki::TaskSetPartition range, uint32_t) {
                auto begin = skinnedView.begin();
                auto end = skinnedView.begin();
                std::advance(begin, range.start);
                std::advance(end, range.end);

                for (auto it = begin; it != end; it++)
                {
                    SkinnedWorldObject& wo = reg.get<SkinnedWorldObject>(*it);

                    for (int i = 0; i < NUM_SUBMESH_MATS; i++)
                    {
                        if (!wo.presentMaterials[i]) continue;
                        loadOrGetMaterial(renderer, wo.materials[i]);
                    }
                }
            });

            g_taskSched.AddTaskSetToPipe(&skinnedLoadMatsTask);
            g_taskSched.WaitforTask(&skinnedLoadMatsTask);
        }

        // Depth Pre-Pass
        cb.BeginDebugLabel("Depth Pre-Pass", 0.1f, 0.1f, 0.1f);

        cb.BindPipeline(depthPrePipeline.Get());
        VK::RenderPass depthPass;
        depthPass.DepthAttachment(depthBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f))
            .RenderArea(rttPass->width, rttPass->height)
            .ViewMask(getViewMask(rttPass->getSettings().numViews));

        depthPass.Begin(cb);

        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), descriptorSets[core->GetFrameIndex()]->GetNativeHandle(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), btm->GetTextureDescriptorSet().GetNativeHandle(), 1);
        cb.SetViewport(VK::Viewport::Simple((float)rttPass->width, (float)rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));

        drawLoop(reg, cb, true, frustums, rttPass->getSettings().numViews);
        depthPass.End(cb);

        cb.EndDebugLabel();

        // Run light culling using the depth buffer
        lightCull->Execute(cb);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderStorageRead, VK::PipelineStageFlags::FragmentShader);

        // Actual "opaque" pass
        cb.BeginDebugLabel("Opaque Pass", 0.5f, 0.1f, 0.1f);
        cb.BindPipeline(pipeline.Get());

        VK::RenderPass colorPass;
        colorPass.ColorAttachment(colorBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(0.0f, 0.0f, 0.0f, 1.0f))
            .DepthAttachment(depthBuffer.Get(), VK::LoadOp::Load, VK::StoreOp::Store)
            .RenderArea(rttPass->width, rttPass->height)
            .ViewMask(getViewMask(rttPass->getSettings().numViews));

        colorPass.Begin(cb);

        drawLoop(reg, cb, false, frustums, rttPass->getSettings().numViews);

        skyboxRenderer->Execute(cb);

        cb.BeginDebugLabel("Debug Lines", 0.1f, 0.1f, 0.1f);
        size_t dbgLinesCount;
        const DebugLine* dbgLines = renderer->getCurrentDebugLines(&dbgLinesCount);

        debugLineDrawer->Execute(cb, dbgLines, dbgLinesCount);
        cb.EndDebugLabel();

        colorPass.End(cb);
        cb.EndDebugLabel();

        // Post-processing
        bloom->Execute(cb);
        tonemapper->Execute(cb);
    }

    void StandardPipeline::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        overrideViews[viewIndex] = viewMatrix;
        overrideProjs[viewIndex] = projectionMatrix;
    }
}