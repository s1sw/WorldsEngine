#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <Core/Engine.hpp>
#include <Core/ConVar.hpp>
#include <Core/MaterialManager.hpp>
#include <Core/TaskScheduler.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VK.hpp>
#include <Render/CullMesh.hpp>
#include <Render/Frustum.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/StandardPipeline/Bloom.hpp>
#include <Render/StandardPipeline/ComputeSkinner.hpp>
#include <Render/StandardPipeline/CubemapConvoluter.hpp>
#include <Render/StandardPipeline/DebugLineDrawer.hpp>
#include <Render/StandardPipeline/LightCull.hpp>
#include <Render/StandardPipeline/HiddenMeshRenderer.hpp>
#include <Render/StandardPipeline/SkyboxRenderer.hpp>
#include <Render/StandardPipeline/Tonemapper.hpp>
#include <Render/StandardPipeline/RenderMaterialManager.hpp>
#include <entt/entity/registry.hpp>
#include <Util/AABB.hpp>
#include <Util/AtomicBufferWrapper.hpp>
#include <Util/JsonUtil.hpp>
#include <Tracy.hpp>

#include <deque>
#include <new>

using namespace R2;

namespace worlds
{
    const uint32_t MAX_DRAWS = 8192;
    struct StandardPushConstants
    {
        uint32_t modelMatrixID;
        uint32_t materialID;
        glm::vec2 textureScale;
        glm::vec2 textureOffset;
    };

    struct LightTile
    {
        uint32_t lightIdMasks[8];
        uint32_t cubemapIdMasks[2];
        uint32_t aoBoxIdMasks[2];
        uint32_t aoSphereIdMasks[2];
    };

    struct GPUDrawInfo
    {
        uint32_t materialOffset;
        uint32_t modelMatrixID;
        glm::vec2 textureOffset;
        glm::vec2 textureScale;
    };

    struct SceneGlobals
    {
        float time;
        glm::vec2 poissonDisk[24];
    };

    static glm::vec2 poissonDisk[24] = {
        glm::vec2(0.511749, 0.547686),  glm::vec2(0.58929, 0.257224),    glm::vec2(0.165018, 0.57663),
        glm::vec2(0.407692, 0.742285),  glm::vec2(0.707012, 0.646523),   glm::vec2(0.31463, 0.466825),
        glm::vec2(0.801257, 0.485186),  glm::vec2(0.418136, 0.146517),   glm::vec2(0.579889, 0.0368284),
        glm::vec2(0.79801, 0.140114),   glm::vec2(-0.0413185, 0.371455), glm::vec2(-0.0529108, 0.627352),
        glm::vec2(0.0821375, 0.882071), glm::vec2(0.17308, 0.301207),    glm::vec2(-0.120452, 0.867216),
        glm::vec2(0.371096, 0.916454),  glm::vec2(-0.178381, 0.146101),  glm::vec2(-0.276489, 0.550525),
        glm::vec2(0.12542, 0.126643),   glm::vec2(-0.296654, 0.286879),  glm::vec2(0.261744, -0.00604975),
        glm::vec2(-0.213417, 0.715776), glm::vec2(0.425684, -0.153211),  glm::vec2(-0.480054, 0.321357)};

    robin_hood::unordered_flat_map<AssetID, int> materialRefCount;
    std::deque<AssetID> convoluteQueue;

    const VK::TextureFormat colorBufferFormat = VK::TextureFormat::B10G11R11_UFLOAT_PACK32;

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

        // Render targets
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

        // Tiled lighting stuff
        VK::BufferCreateInfo lightTileBci{
            VK::BufferUsage::Storage,
            sizeof(LightTile) * ((rttPass->width + 31) / 32) * ((rttPass->height + 31) / 32) * settings.numViews,
            true};
        lightTileBuffer = core->CreateBuffer(lightTileBci);
        lightTileBuffer->SetDebugName("Light Tile Buffer");

        for (int i = 0; i < 2; i++)
        {
            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            dsu.AddBuffer(4, 0, VK::DescriptorType::StorageBuffer, lightTileBuffer.Get());
            dsu.Update();
        }

        lightCull =
            new LightCull(core, depthBuffer.Get(), lightBuffers.Get(), lightTileBuffer.Get(), multiVPBuffer.Get());
        debugLineDrawer = new DebugLineDrawer(
            core, multiVPBuffer.Get(), rttPass->getSettings().msaaLevel, getViewMask(settings.numViews));
        bloom = new Bloom(core, colorBuffer.Get());
        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget(), bloom->GetOutput());
        skyboxRenderer =
            new SkyboxRenderer(core, pipelineLayout.Get(), settings.msaaLevel, getViewMask(settings.numViews));
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

        if (!RenderMaterialManager::IsInitialized())
            RenderMaterialManager::Initialize(renderer);

        VK::BufferCreateInfo vpBci{VK::BufferUsage::Uniform, sizeof(MultiVP), true};
        multiVPBuffer = core->CreateBuffer(vpBci);
        multiVPBuffer->SetDebugName("View Info Buffer");

        VK::BufferCreateInfo modelMatrixBci{VK::BufferUsage::Storage, sizeof(glm::mat4) * 4096, true};
        modelMatrixBuffers = new VK::FrameSeparatedBuffer(core, modelMatrixBci);

        VK::BufferCreateInfo lightBci{VK::BufferUsage::Storage, sizeof(LightUB), true};
        lightBuffers = new VK::FrameSeparatedBuffer(core, lightBci);

        VK::BufferCreateInfo drawInfoBCI{VK::BufferUsage::Storage, sizeof(GPUDrawInfo) * MAX_DRAWS, true};
        drawInfoBuffers = new VK::FrameSeparatedBuffer(core, drawInfoBCI);

        VK::BufferCreateInfo globalsBCI{VK::BufferUsage::Uniform, sizeof(SceneGlobals), true};
        sceneGlobals = core->CreateBuffer(globalsBCI);
        core->QueueBufferUpload(
            sceneGlobals.Get(), poissonDisk, sizeof(poissonDisk), offsetof(SceneGlobals, poissonDisk));

        VK::BufferCreateInfo drawCmdsBCI{
            VK::BufferUsage::Indirect, sizeof(VK::DrawIndexedIndirectCommand) * MAX_DRAWS, true};
        drawCommandBuffers = new VK::FrameSeparatedBuffer(core, drawCmdsBCI);

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
        dslb.Binding(5, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(6, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Fragment);
        descriptorSetLayout = dslb.Build();

        for (int i = 0; i < 2; i++)
        {
            descriptorSets[i] = core->CreateDescriptorSet(descriptorSetLayout.Get());

            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, multiVPBuffer.Get());
            dsu.AddBuffer(1, 0, VK::DescriptorType::StorageBuffer, modelMatrixBuffers->GetBuffer(i));
            dsu.AddBuffer(2, 0, VK::DescriptorType::StorageBuffer, RenderMaterialManager::GetBuffer());
            dsu.AddBuffer(3, 0, VK::DescriptorType::StorageBuffer, lightBuffers->GetBuffer(i));
            dsu.AddBuffer(5, 0, VK::DescriptorType::StorageBuffer, drawInfoBuffers->GetBuffer(i));
            dsu.AddBuffer(6, 0, VK::DescriptorType::UniformBuffer, sceneGlobals.Get());
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

        if (settings.outputToXR)
            hiddenMeshRenderer = new HiddenMeshRenderer(core, settings.msaaLevel);
        
        computeSkinner = new ComputeSkinner(renderer);
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

    struct LoadCubemapsTask : public enki::ITaskSet
    {
        LightUB* lightUB;
        entt::registry& registry;
        VKTextureManager* textureManager;

        LoadCubemapsTask(LightUB* lightUB, entt::registry& registry, VKTextureManager* textureManager)
            : lightUB(lightUB), registry(registry), textureManager(textureManager)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            ZoneScoped;

            auto cubemapView = registry.view<WorldCubemap>();

            auto begin = cubemapView.begin();
            auto end = cubemapView.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                WorldCubemap& wc = registry.get<WorldCubemap>(*it);
                if (!wc.isLoaded)
                {
                    lightUB->cubemaps[wc.renderIdx].texture = textureManager->loadAndGet(wc.cubemapId);
                    wc.isLoaded = true;
                }
                else
                {
                    lightUB->cubemaps[wc.renderIdx].texture = textureManager->get(wc.cubemapId);
                }
            }
        }
    };

    struct FillLightBufferTask : public enki::ITaskSet
    {
        LightUB* lightUB;
        entt::registry& registry;
        VKTextureManager* textureManager;
        ShadowmapManager* shadowmapManager;

        FillLightBufferTask(LightUB* lightUB, entt::registry& registry, VKTextureManager* textureManager)
            : lightUB(lightUB), registry(registry), textureManager(textureManager),
              lcTask(lightUB, registry, textureManager)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            ZoneScoped;

            uint32_t lightCount = 0;
            registry.view<WorldLight, Transform>().each([&](WorldLight& wl, const Transform& t) {
                if (!wl.enabled)
                    return;

                if (wl.shadowmapIdx != ~0u)
                {
                    Camera shadowCam{};
                    shadowCam.position = t.position;
                    shadowCam.rotation = t.rotation;
                    shadowCam.verticalFOV = wl.spotOuterCutoff * 2.0f;
                    shadowCam.near = wl.shadowNear;
                    shadowCam.far = wl.shadowFar;

                    glm::mat4 vp = shadowCam.getProjectMatrixNonInfinite(1.0f) * shadowCam.getViewMatrix();
                    lightUB->additionalShadowMatrices[wl.shadowmapIdx] = vp;
                    lightUB->shadowmapIds[wl.shadowmapIdx] = shadowmapManager->GetShadowmapId(wl.shadowmapIdx);
                }

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
                pl.setShadowmapIndex(wl.shadowmapIdx);

                lightUB->lights[lightCount] = pl;
                lightCount++;
            });

            lightUB->lightCount = lightCount;

            AssetID skybox = registry.ctx<SceneSettings>().skybox;

            if (!textureManager->isLoaded(skybox))
            {
                convoluteQueue.push_back(skybox);
                textureManager->loadAndGet(skybox);
            }

            uint32_t cubemapIdx = 1;
            lightUB->cubemaps[0] = GPUCubemap{glm::vec3{100000.0f}, textureManager->get(skybox), glm::vec3{0.0f}, 0};

            registry.view<WorldCubemap, Transform>().each([&](WorldCubemap& wc, Transform& t) {
                GPUCubemap gc{};
                gc.extent = wc.extent;
                gc.position = t.position;
                gc.flags = wc.cubeParallax;
                if (!textureManager->isLoaded(wc.cubemapId))
                {
                    convoluteQueue.push_back(wc.cubemapId);
                }
                lightUB->cubemaps[cubemapIdx] = gc;
                wc.renderIdx = cubemapIdx;
                cubemapIdx++;
            });

            lightUB->cubemapCount = cubemapIdx;

            lcTask.m_SetSize = registry.view<WorldCubemap>().size();
            g_taskSched.AddTaskSetToPipe(&lcTask);
            g_taskSched.WaitforTask(&lcTask);
        }

    private:
        LoadCubemapsTask lcTask;
    };

    struct WorldObjectMaterialLoadTask : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;

        WorldObjectMaterialLoadTask(VKRenderer* renderer, entt::registry& reg) : renderer(renderer), reg(reg)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            auto view = reg.view<WorldObject>();

            auto begin = view.begin();
            auto end = view.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                WorldObject& wo = reg.get<WorldObject>(*it);
                const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(wo.mesh);

                for (int i = 0; i < rmi.numSubmeshes; i++)
                {
                    const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];
                    uint8_t materialIndex = rsi.materialIndex;

                    if (!wo.presentMaterials[materialIndex])
                        materialIndex = 0;

                    if (RenderMaterialManager::IsMaterialLoaded(wo.materials[materialIndex]))
                        continue;

                    RenderMaterialManager::LoadOrGetMaterial(wo.materials[materialIndex]);
                }
            }
        }
    };

    struct SkinnedWorldObjectMaterialLoadTask : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;

        SkinnedWorldObjectMaterialLoadTask(VKRenderer* renderer, entt::registry& reg) : renderer(renderer), reg(reg)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            auto view = reg.view<SkinnedWorldObject>();

            auto begin = view.begin();
            auto end = view.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                WorldObject& wo = reg.get<SkinnedWorldObject>(*it);

                for (int i = 0; i < NUM_SUBMESH_MATS; i++)
                {
                    if (!wo.presentMaterials[i])
                        continue;

                    if (RenderMaterialManager::IsMaterialLoaded(wo.materials[i]))
                        continue;

                    RenderMaterialManager::LoadOrGetMaterial(wo.materials[i]);
                }
            }
        }
    };

    struct FillDrawBufferTask : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;
        int numViews;
        Frustum* frustums;
        AtomicBufferWrapper<glm::mat4>* modelMatrices;
        GPUDrawInfo* gpuDrawInfos;
        VK::DrawIndexedIndirectCommand* drawCmds;
        std::atomic<uint32_t> drawIdCounter = 0;

        FillDrawBufferTask(VKRenderer* renderer, entt::registry& reg) : renderer(renderer), reg(reg)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            auto view = reg.view<WorldObject>();

            auto begin = view.begin();
            auto end = view.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                WorldObject& wo = reg.get<WorldObject>(*it);
                const Transform& t = reg.get<Transform>(*it);
                const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(wo.mesh);

                // Cull against frustum
                if (!cullMesh(rmi, t, frustums, numViews))
                    continue;

                uint32_t modelMatrixIdx = modelMatrices->Append(t.getMatrix());

                for (int i = 0; i < rmi.numSubmeshes; i++)
                {
                    if (!wo.drawSubmeshes[i]) continue;

                    const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];

                    AssetID material = wo.materials[rsi.materialIndex];

                    if (!wo.presentMaterials[rsi.materialIndex])
                        material = wo.materials[0];

                    if (!RenderMaterialManager::IsMaterialLoaded(material))
                        continue;

                    uint32_t drawId = drawIdCounter.fetch_add(1);

                    GPUDrawInfo di{};
                    di.materialOffset = RenderMaterialManager::GetMaterial(material);
                    di.modelMatrixID = modelMatrixIdx;
                    di.textureScale = glm::vec2(wo.texScaleOffset);
                    di.textureOffset = glm::vec2(wo.texScaleOffset.z, wo.texScaleOffset.w);

                    VK::DrawIndexedIndirectCommand drawCmd{};
                    drawCmd.indexCount = rsi.indexCount;
                    drawCmd.firstIndex = rsi.indexOffset + (rmi.indexOffset / sizeof(uint32_t));
                    drawCmd.firstInstance = 0;
                    drawCmd.instanceCount = 1;
                    drawCmd.vertexOffset = rmi.vertsOffset / sizeof(Vertex);

                    drawCmds[drawId] = drawCmd;
                    gpuDrawInfos[drawId] = di;
                }
            }
        }
    };

    struct AllocatedSkinnedStorageTask : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;
        std::atomic<uint32_t> vertCounter;

        AllocatedSkinnedStorageTask(VKRenderer* renderer, entt::registry& reg)
            : renderer(renderer), reg(reg), vertCounter(0)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            auto view = reg.view<SkinnedWorldObject>();

            auto begin = view.begin();
            auto end = view.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                SkinnedWorldObject& wo = reg.get<SkinnedWorldObject>(*it);
                const Transform& t = reg.get<Transform>(*it);
                const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(wo.mesh);

                wo.skinnedVertexOffset = vertCounter.fetch_add(rmi.numVertices);
            }
        }
    };

    struct FillDrawBufferSkinnedTask : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;
        int numViews;
        Frustum* frustums;
        AtomicBufferWrapper<glm::mat4>* modelMatrices;
        GPUDrawInfo* gpuDrawInfos;
        VK::DrawIndexedIndirectCommand* drawCmds;
        std::atomic<uint32_t>& drawIdCounter;

        FillDrawBufferSkinnedTask(VKRenderer* renderer, entt::registry& reg, std::atomic<uint32_t>& drawIdCounter)
            : renderer(renderer), reg(reg), drawIdCounter(drawIdCounter)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            auto view = reg.view<SkinnedWorldObject>();

            auto begin = view.begin();
            auto end = view.begin();
            std::advance(begin, range.start);
            std::advance(end, range.end);

            for (auto it = begin; it != end; it++)
            {
                SkinnedWorldObject& wo = reg.get<SkinnedWorldObject>(*it);
                const Transform& t = reg.get<Transform>(*it);
                const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(wo.mesh);

                // Cull against frustum
                if (!cullMesh(rmi, t, frustums, numViews))
                    continue;

                uint32_t modelMatrixIdx = modelMatrices->Append(t.getMatrix());

                for (int i = 0; i < rmi.numSubmeshes; i++)
                {
                    const RenderSubmeshInfo& rsi = rmi.submeshInfo[i];

                    AssetID material = wo.materials[rsi.materialIndex];

                    if (!wo.presentMaterials[rsi.materialIndex])
                        material = wo.materials[0];

                    if (!RenderMaterialManager::IsMaterialLoaded(material))
                        continue;

                    uint32_t drawId = drawIdCounter.fetch_add(1);

                    GPUDrawInfo di{};
                    di.materialOffset = RenderMaterialManager::GetMaterial(material);
                    di.modelMatrixID = modelMatrixIdx;
                    di.textureScale = glm::vec2(wo.texScaleOffset);
                    di.textureOffset = glm::vec2(wo.texScaleOffset.z, wo.texScaleOffset.w);

                    VK::DrawIndexedIndirectCommand drawCmd{};
                    drawCmd.indexCount = rsi.indexCount;
                    drawCmd.firstIndex = rsi.indexOffset + (rmi.indexOffset / sizeof(uint32_t));
                    drawCmd.firstInstance = 0;
                    drawCmd.instanceCount = 1;
                    drawCmd.vertexOffset = wo.skinnedVertexOffset + (renderer->getMeshManager()->getSkinnedVertsOffset() / sizeof(Vertex));

                    drawCmds[drawId] = drawCmd;
                    gpuDrawInfos[drawId] = di;
                }
            }
        }
    };

    struct GCResources : public enki::ITaskSet
    {
        VKRenderer* renderer;
        entt::registry& reg;
    };

    struct TasksFinished : public enki::ICompletable
    {
        std::vector<enki::Dependency> dependencies;
    };

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        ZoneScoped;
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();
        uint32_t frameIdx = renderer->getCore()->GetFrameIndex();
        // RenderMaterialManager::UnloadUnusedMaterials(reg);

        SceneGlobals* globals = (SceneGlobals*)sceneGlobals->Map();
        globals->time = renderer->getTime();
        sceneGlobals->Unmap();

        // If there's anything in the convolution queue, convolute 1 cubemap
        // per frame (convolution is slow!)
        if (convoluteQueue.size() > 0)
        {
            cb.BeginDebugLabel("Cubemap Convolution", 0.1f, 0.1f, 0.1f);

            AssetID convoluteID = convoluteQueue.front();
            convoluteQueue.pop_front();
            uint32_t convoluteHandle = textureManager->loadAndGet(convoluteID);
            VK::Texture* tex = btm->GetTextureAt(convoluteHandle);
            cubemapConvoluter->Convolute(cb, tex);

            cb.EndDebugLabel();
        }

        TasksFinished finisher;

        LightUB* lightUB = (LightUB*)lightBuffers->MapCurrent();

        FillLightBufferTask fillTask{lightUB, reg, textureManager};
        fillTask.shadowmapManager = renderer->getShadowmapManager();

        WorldObjectMaterialLoadTask woLoadTask{renderer, reg};
        woLoadTask.m_SetSize = reg.view<WorldObject>().size();

        SkinnedWorldObjectMaterialLoadTask swoLoadTask{renderer, reg};
        swoLoadTask.m_SetSize = reg.view<SkinnedWorldObject>().size();

        AllocatedSkinnedStorageTask allocSkinnedStorageTask{renderer, reg};
        allocSkinnedStorageTask.m_SetSize = reg.view<SkinnedWorldObject>().size();
        allocSkinnedStorageTask.m_MinRange = 10;

        finisher.SetDependenciesVec<std::vector<enki::Dependency>, enki::ITaskSet>(
            finisher.dependencies, {&fillTask, &woLoadTask, &swoLoadTask, &allocSkinnedStorageTask});

        g_taskSched.AddTaskSetToPipe(&fillTask);
        g_taskSched.AddTaskSetToPipe(&woLoadTask);
        g_taskSched.AddTaskSetToPipe(&swoLoadTask);
        g_taskSched.AddTaskSetToPipe(&allocSkinnedStorageTask);
        g_taskSched.WaitforTask(&finisher);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderStorageRead, VK::PipelineStageFlags::FragmentShader);
        modelMatrixBuffers->GetCurrentBuffer()->Acquire(
            cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::VertexShader);

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

        computeSkinner->Execute(cb, reg);

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrixBuffers->MapCurrent();
        GPUDrawInfo* drawInfosMapped = (GPUDrawInfo*)drawInfoBuffers->MapCurrent();
        VK::DrawIndexedIndirectCommand* drawCmdsMapped =
            (VK::DrawIndexedIndirectCommand*)drawCommandBuffers->MapCurrent();

        AtomicBufferWrapper<glm::mat4> matrixWrapper{modelMatricesMapped};

        FillDrawBufferTask fdbTask{renderer, reg};
        fdbTask.numViews = rttPass->getSettings().numViews;
        fdbTask.frustums = frustums;
        fdbTask.modelMatrices = &matrixWrapper;
        fdbTask.gpuDrawInfos = drawInfosMapped;
        fdbTask.drawCmds = drawCmdsMapped;

        fdbTask.m_SetSize = reg.view<WorldObject>().size();

        FillDrawBufferSkinnedTask fdbsTask{renderer, reg, fdbTask.drawIdCounter};
        fdbsTask.numViews = rttPass->getSettings().numViews;
        fdbsTask.frustums = frustums;
        fdbsTask.modelMatrices = &matrixWrapper;
        fdbsTask.gpuDrawInfos = drawInfosMapped;
        fdbsTask.drawCmds = drawCmdsMapped;

        fdbsTask.m_SetSize = reg.view<SkinnedWorldObject>().size();

        TasksFinished finisher2;
        finisher2.SetDependenciesVec<std::vector<enki::Dependency>, enki::ITaskSet>(
            finisher2.dependencies, {&fdbTask, &fdbsTask});

        g_taskSched.AddTaskSetToPipe(&fdbTask);
        g_taskSched.AddTaskSetToPipe(&fdbsTask);
        g_taskSched.WaitforTask(&finisher2);

        modelMatrixBuffers->UnmapCurrent();
        drawInfoBuffers->UnmapCurrent();
        drawCommandBuffers->UnmapCurrent();

        // Depth Pre-Pass
        cb.BeginDebugLabel("Depth Pre-Pass", 0.1f, 0.1f, 0.1f);

        VK::RenderPass depthPass;
        depthPass.DepthAttachment(depthBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
            .DepthAttachmentClearValue(VK::ClearValue::DepthClear(0.0f))
            .RenderArea(rttPass->width, rttPass->height)
            .ViewMask(getViewMask(rttPass->getSettings().numViews));

        depthPass.Begin(cb);

        cb.SetViewport(VK::Viewport::Simple((float)rttPass->width, (float)rttPass->height));
        cb.SetScissor(VK::ScissorRect::Simple(rttPass->width, rttPass->height));
        if (rttPass->getSettings().outputToXR)
            hiddenMeshRenderer->Execute(cb);

        cb.BindPipeline(depthPrePipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), descriptorSets[core->GetFrameIndex()]->GetNativeHandle(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), btm->GetTextureDescriptorSet().GetNativeHandle(), 1);
        cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), 0);
        cb.BindIndexBuffer(meshManager->getIndexBuffer(), 0, VK::IndexType::Uint32);

        cb.DrawIndexedIndirect(
            drawCommandBuffers->GetCurrentBuffer(), 0, fdbTask.drawIdCounter, sizeof(VK::DrawIndexedIndirectCommand));
        depthPass.End(cb);
        renderer->getDebugStats().numDrawCalls = fdbTask.drawIdCounter;

        cb.EndDebugLabel();

        // Run light culling using the depth buffer
        lightCull->Execute(cb);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::FragmentShader);

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

        cb.DrawIndexedIndirect(
            drawCommandBuffers->GetCurrentBuffer(), 0, fdbTask.drawIdCounter, sizeof(VK::DrawIndexedIndirectCommand));

        skyboxRenderer->Execute(cb);

        cb.BeginDebugLabel("Debug Lines", 0.1f, 0.1f, 0.1f);
        size_t dbgLinesCount;
        const DebugLine* dbgLines = renderer->getCurrentDebugLines(&dbgLinesCount);

        debugLineDrawer->Execute(cb, dbgLines, dbgLinesCount);
        cb.EndDebugLabel();

        colorPass.End(cb);
        cb.EndDebugLabel();

        // Post-processing
        static ConVar r_skipBloom{"r_skipBloom", "0", "Skips bloom rendering."};
        if (!r_skipBloom)
            bloom->Execute(cb);

        tonemapper->Execute(cb, r_skipBloom);

        lightBuffers->UnmapCurrent();
    }

    void StandardPipeline::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        overrideViews[viewIndex] = viewMatrix;
        overrideProjs[viewIndex] = projectionMatrix;
    }
}