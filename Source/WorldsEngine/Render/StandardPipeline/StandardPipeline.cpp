#include "StandardPipeline.hpp"
#include <Core/AssetDB.hpp>
#include <Core/Engine.hpp>
#include <Core/ConVar.hpp>
#include <Core/MaterialManager.hpp>
#include <Core/TaskScheduler.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VKTimestampPool.hpp>
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
#include <Render/RenderMaterialManager.hpp>
#include <Render/StandardPipeline/ParticleRenderer.hpp>
#include <entt/entity/registry.hpp>
#include <Util/AABB.hpp>
#include <Util/AtomicBufferWrapper.hpp>
#include <Util/EnumUtil.hpp>
#include <Util/JsonUtil.hpp>
#include <Tracy.hpp>

#include <readerwriterqueue.h>
#include <new>
#include <Core/Fatal.hpp>
#include "PoissonDisk.hpp"

using namespace R2;

namespace worlds
{
    enum TimestampStage
    {
        TS_Skinning,
        TS_DepthPrepass,
        TS_LightCull,
        TS_MainPass,
        TS_Bloom,
        TS_Tonemap,
        TS_Count
    };

    const int NUM_TIMESTAMPS = TS_Count * 2;

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
        float shadowmapResolution;
        uint32_t blueNoiseTexture;
        glm::vec2 poissonDisk[64];
    };

    robin_hood::unordered_flat_map<AssetID, int> materialRefCount;
    robin_hood::unordered_set<AssetID> allCubemaps;
    moodycamel::ReaderWriterQueue<AssetID> convoluteQueue;

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

    StandardPipeline::StandardPipeline(const EngineInterfaces& engineInterfaces)
        : engineInterfaces(engineInterfaces)
    {
        drawCmds.resize(MAX_DRAWS);
    }

    StandardPipeline::~StandardPipeline()
    {
        ((VKRenderer*)engineInterfaces.renderer)->getTextureManager()->release(
            AssetDB::pathToId("Textures/bluenoise.png"));
    }

    void StandardPipeline::createSizeDependants()
    {
        ZoneScoped;

        const RTTPassSettings& settings = rttPass->getSettings();
        VK::Core* core = ((VKRenderer*)engineInterfaces.renderer)->getCore();
        depthBuffer.Reset();
        colorBuffer.Reset();

        // Render targets
        VK::TextureCreateInfo depthBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::D32_SFLOAT, rttPass->width, rttPass->height);

        depthBufferCI.Samples = rttPass->getSettings().msaaLevel;
        depthBufferCI.Layers = settings.numViews;
        if (settings.numViews > 1) depthBufferCI.Dimension = VK::TextureDimension::Array2D;
        depthBuffer = core->CreateTexture(depthBufferCI);
        depthBuffer->SetDebugName("Depth Buffer");

        VK::TextureCreateInfo colorBufferCI =
            VK::TextureCreateInfo::RenderTarget2D(colorBufferFormat, rttPass->width, rttPass->height);

        colorBufferCI.Samples = rttPass->getSettings().msaaLevel;
        colorBufferCI.Layers = settings.numViews;
        if (settings.numViews > 1) colorBufferCI.Dimension = VK::TextureDimension::Array2D;
        colorBuffer = core->CreateTexture(colorBufferCI);
        colorBuffer->SetDebugName("Color Buffer");

        // Tiled lighting stuff
        VK::BufferCreateInfo lightTileBci{
            VK::BufferUsage::Storage,
            sizeof(LightTile) * ((rttPass->width + 31) / 32) * ((rttPass->height + 31) / 32) * settings.numViews,
            true
        };
        lightTileBuffer = core->CreateBuffer(lightTileBci);
        lightTileBuffer->SetDebugName("Light Tile Buffer");

        for (int i = 0; i < 2; i++)
        {
            VK::DescriptorSetUpdater dsu{core, descriptorSets[i].Get()};
            dsu.AddBuffer(4, 0, VK::DescriptorType::StorageBuffer, lightTileBuffer.Get());
            dsu.Update();
        }

        lightCull =
            new LightCull((VKRenderer*)engineInterfaces.renderer, depthBuffer.Get(),
                          lightBuffers.Get(), lightTileBuffer.Get(), multiVPBuffer.Get());

        debugLineDrawer = new DebugLineDrawer(
            core, multiVPBuffer.Get(), rttPass->getSettings().msaaLevel, getViewMask(settings.numViews));
        bloom = new Bloom(core, colorBuffer.Get());
        tonemapper = new Tonemapper(core, colorBuffer.Get(), rttPass->getFinalTarget(), bloom->GetOutput());
        skyboxRenderer =
            new SkyboxRenderer(core, pipelineLayout.Get(), settings.msaaLevel, getViewMask(settings.numViews));
    }

    void StandardPipeline::setupMainPassPipeline(VK::PipelineBuilder& pb, VK::VertexBinding& vb)
    {
        pb.PrimitiveTopology(VK::Topology::TriangleList)
          .CullMode(VK::CullMode::Back)
          .Layout(pipelineLayout.Get())
          .ColorAttachmentFormat(colorBufferFormat)
          .AddVertexBinding(vb)
          .DepthTest(true)
          .DepthWrite(false)
          .DepthCompareOp(VK::CompareOp::Equal)
          .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
          .MSAASamples(rttPass->getSettings().msaaLevel);
        pb.ViewMask(getViewMask(rttPass->getSettings().numViews));
    }

    void StandardPipeline::setupDepthPassPipeline(VK::PipelineBuilder& pb, VK::VertexBinding& vb)
    {
        pb.PrimitiveTopology(VK::Topology::TriangleList)
          .CullMode(VK::CullMode::Back)
          .Layout(pipelineLayout.Get())
          .AddVertexBinding(vb)
          .DepthTest(true)
          .DepthWrite(true)
          .DepthCompareOp(VK::CompareOp::Greater)
          .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
          .MSAASamples(rttPass->getSettings().msaaLevel);

        pb.ViewMask(getViewMask(rttPass->getSettings().numViews));
    }

    void StandardPipeline::setup(VKRTTPass* rttPass)
    {
        ZoneScoped;
        const RTTPassSettings& settings = rttPass->getSettings();
        VKRenderer* renderer = (VKRenderer*)engineInterfaces.renderer;
        VK::Core* core = ((VKRenderer*)engineInterfaces.renderer)->getCore();
        this->rttPass = rttPass;

        if (rttPass->getSettings().numViews > 1)
        {
            useViewOverrides = true;
            overrideViews.resize(rttPass->getSettings().numViews);
            overrideProjs.resize(rttPass->getSettings().numViews);
        }

        if (!RenderMaterialManager::IsInitialized())
            RenderMaterialManager::Initialize(renderer);

        techniqueManager = new TechniqueManager;

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
            sceneGlobals.Get(), poissonDisk, sizeof(glm::vec2) * 64, offsetof(SceneGlobals, poissonDisk));
        SceneGlobals* globals = (SceneGlobals*)sceneGlobals->Map();
        globals->blueNoiseTexture = renderer->getTextureManager()->loadSynchronous(
            AssetDB::pathToId("Textures/bluenoise.png"));
        sceneGlobals->Unmap();

        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(1, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(2, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(3, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(4, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.UpdateAfterBind();
        dslb.Binding(5, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(6, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        descriptorSetLayout = dslb.Build();

        for (int i = 0; i < 2; i++)
        {
            descriptorSets[i] = core->CreateDescriptorSet(descriptorSetLayout.Get());

            VK::DescriptorSetUpdater dsu{core, descriptorSets[i].Get()};
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

        AssetID vs = AssetDB::pathToId("Shaders/standard.vert.spv");
        AssetID fs = AssetDB::pathToId("Shaders/standard.frag.spv");
        AssetID depthFS = AssetDB::pathToId("Shaders/standard_empty.frag.spv");
        AssetID depthAlphaTestFS = AssetDB::pathToId("Shaders/standard_alpha_test.frag.spv");

        VK::ShaderModule& stdVert = ShaderCache::getModule(vs);
        VK::ShaderModule& stdFrag = ShaderCache::getModule(fs);
        VK::ShaderModule& depthFrag = ShaderCache::getModule(depthFS);
        VK::ShaderModule& depthAlphaTestFrag = ShaderCache::getModule(depthAlphaTestFS);

        standardTechnique = techniqueManager->createTechnique();

        VK::PipelineBuilder pb{core};
        setupMainPassPipeline(pb, vb);
        pb.AddShader(VK::ShaderStage::Vertex, stdVert)
          .AddShader(VK::ShaderStage::Fragment, stdFrag);

        techniqueManager->registerVariant(standardTechnique, pb.Build(), VariantFlags::None);
        techniqueManager->registerVariant(standardTechnique, pb.Build(), VariantFlags::AlphaTest);

        VK::PipelineBuilder pb2{core};
        setupDepthPassPipeline(pb2, vb);
        pb2.AddShader(VK::ShaderStage::Vertex, stdVert)
           .AddShader(VK::ShaderStage::Fragment, depthFrag);

        techniqueManager->registerVariant(standardTechnique, pb2.Build(), VariantFlags::DepthPrepass);

        VK::PipelineBuilder pb3{core};
        setupDepthPassPipeline(pb3, vb);
        pb3.AddShader(VK::ShaderStage::Vertex, stdVert)
           .AddShader(VK::ShaderStage::Fragment, depthAlphaTestFrag)
           .AlphaToCoverage(true);
        techniqueManager->registerVariant(standardTechnique, pb3.Build(),
                                          VariantFlags::DepthPrepass | VariantFlags::AlphaTest);

        cubemapConvoluter = new CubemapConvoluter(core);
        createSizeDependants();

        if (settings.outputToXR)
            hiddenMeshRenderer = new HiddenMeshRenderer(engineInterfaces, settings.msaaLevel, multiVPBuffer.Get());

        computeSkinner = new ComputeSkinner(renderer);
        particleRenderer = new ParticleRenderer(renderer, settings.msaaLevel, getViewMask(settings.numViews),
                                                multiVPBuffer.Get());
        timestampPool = new VK::TimestampPool(
            core->GetHandles(), (int)core->GetNumFramesInFlight() * NUM_TIMESTAMPS);
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

    struct FillLightBufferTask : public enki::ITaskSet
    {
        LightUB* lightUB;
        entt::registry& registry;
        VKTextureManager* textureManager;
        ShadowmapManager* shadowmapManager;
        int numViews;
        Frustum* frustums;
        RenderDebugStats* dbgStats;

        FillLightBufferTask(LightUB* lightUB, entt::registry& registry, VKTextureManager* textureManager)
            : lightUB(lightUB), registry(registry), textureManager(textureManager)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            ZoneScoped;

            uint32_t lightCount = 0;
            registry.view<WorldLight, Transform>().each([&](WorldLight& wl, const Transform& t)
            {
                if (!wl.enabled || lightCount >= LightUB::MAX_LIGHTS)
                    return;

                for (int i = 0; i < numViews; i++)
                {
                    if (!frustums[i].containsSphere(t.position, wl.maxDistance)) return;
                }

                if (wl.shadowmapIdx != ~0u)
                {
                    lightUB->additionalShadowMatrices[wl.shadowmapIdx] = shadowmapManager->GetShadowVPMatrix(
                        wl.shadowmapIdx);
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
                pl.shadowBias = wl.shadowBias;

                lightUB->lights[lightCount] = pl;
                lightCount++;
            });

            lightUB->lightCount = lightCount;
            dbgStats->numLightsInView = lightCount;

            AssetID skybox = registry.ctx<SkySettings>().skybox;

            if (!allCubemaps.contains(skybox))
            {
                allCubemaps.insert(skybox);
            }

            uint32_t cubemapIdx = 1;
            lightUB->cubemaps[0] = GPUCubemap{glm::vec3{100000.0f}, textureManager->get(skybox), glm::vec3{0.0f}, 0};

            registry.view<WorldCubemap, Transform>().each([&](WorldCubemap& wc, Transform& t)
            {
                GPUCubemap gc{};
                gc.extent = wc.extent;
                gc.position = t.position;
                gc.flags = wc.cubeParallax;
                gc.blendDistance = wc.blendDistance;
                if (!textureManager->isLoaded(wc.cubemapId))
                {
                    allCubemaps.insert(wc.cubemapId);
                }
                if (textureManager->isLoaded(wc.cubemapId))
                    gc.texture = textureManager->get(wc.cubemapId);
                else
                    gc.texture = textureManager->get(skybox);
                lightUB->cubemaps[cubemapIdx] = gc;
                wc.renderIdx = cubemapIdx;
                cubemapIdx++;
            });

            lightUB->cubemapCount = cubemapIdx;
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
        StandardDrawCommand* drawCmds;
        std::atomic<uint32_t> drawIdCounter = 0;
        bool onlyStatics = false;
        robin_hood::unordered_map<uint32_t, uint32_t>* customShaderTechniques;

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

                if (onlyStatics && !enumHasFlag(wo.staticFlags, StaticFlags::Rendering))
                    continue;

                const Transform& t = reg.get<Transform>(*it);
                RenderMeshInfo* rmi;
                if (!renderer->getMeshManager()->get(wo.mesh, &rmi))
                    continue;

                // Cull against frustum
                if (!cullMesh(*rmi, t, frustums, numViews))
                    continue;

                uint32_t modelMatrixIdx = modelMatrices->Append(t.getMatrix());

                for (int i = 0; i < rmi->numSubmeshes; i++)
                {
                    if (!wo.drawSubmeshes[i]) continue;

                    const RenderSubmeshInfo& rsi = rmi->submeshInfo[i];

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

                    const MaterialInfo& materialInfo = RenderMaterialManager::GetMaterialInfo(material);
                    StandardDrawCommand drawCmd{};
                    drawCmd.indexCount = rsi.indexCount;
                    drawCmd.firstIndex = rsi.indexOffset + (rmi->indexOffset / sizeof(uint32_t));
                    drawCmd.vertexOffset = rmi->vertsOffset / sizeof(Vertex);
                    drawCmd.variantFlags = materialInfo.alphaTest ? VariantFlags::AlphaTest : VariantFlags::None;

                    if (materialInfo.fragmentShader != INVALID_ASSET || materialInfo.vertexShader != INVALID_ASSET)
                    {
                        AssetID fragShaderID = materialInfo.fragmentShader;
                        AssetID vertShaderID = materialInfo.vertexShader;
                        AssetID standardFragID = AssetDB::pathToId("Shaders/standard.frag.spv");
                        AssetID standardVertID = AssetDB::pathToId("Shaders/standard.vert.spv");

                        if (fragShaderID == INVALID_ASSET)
                            fragShaderID = standardFragID;

                        if (vertShaderID == INVALID_ASSET)
                            vertShaderID = standardVertID;

                        // Based on https://stackoverflow.com/a/2595226
                        uint32_t key = vertShaderID;
                        key ^= fragShaderID + 0x9e3779b9 + (key << 6) + (key >> 2);

                        releaseAssert(customShaderTechniques->contains(key));
                        drawCmd.techniqueIdx = customShaderTechniques->at(key);
                    }

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
        StandardDrawCommand* drawCmds;
        std::atomic<uint32_t>& drawIdCounter;
        bool onlyStatics = false;
        robin_hood::unordered_map<uint32_t, uint32_t>* customShaderTechniques;

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

                if (onlyStatics && !enumHasFlag(wo.staticFlags, StaticFlags::Rendering))
                    continue;

                const Transform& t = reg.get<Transform>(*it);
                const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(wo.mesh);

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

                    const MaterialInfo& materialInfo = RenderMaterialManager::GetMaterialInfo(material);
                    StandardDrawCommand drawCmd{};
                    drawCmd.indexCount = rsi.indexCount;
                    drawCmd.firstIndex = rsi.indexOffset + (rmi.indexOffset / sizeof(uint32_t));
                    drawCmd.vertexOffset = wo.skinnedVertexOffset + (renderer->getMeshManager()->getSkinnedVertsOffset()
                        / sizeof(Vertex));
                    drawCmd.variantFlags = materialInfo.alphaTest ? VariantFlags::AlphaTest : VariantFlags::None;

                    if (materialInfo.fragmentShader != INVALID_ASSET || materialInfo.vertexShader != INVALID_ASSET)
                    {
                        AssetID fragShaderID = materialInfo.fragmentShader;
                        AssetID vertShaderID = materialInfo.vertexShader;
                        AssetID standardFragID = AssetDB::pathToId("Shaders/standard.frag.spv");
                        AssetID standardVertID = AssetDB::pathToId("Shaders/standard.vert.spv");

                        if (fragShaderID == INVALID_ASSET)
                            fragShaderID = standardFragID;

                        if (vertShaderID == INVALID_ASSET)
                            vertShaderID = standardVertID;

                        // Based on https://stackoverflow.com/a/2595226
                        uint32_t key = vertShaderID;
                        key ^= fragShaderID + 0x9e3779b9 + (key << 6) + (key >> 2);

                        releaseAssert(customShaderTechniques->contains(key));
                        drawCmd.techniqueIdx = customShaderTechniques->at(key);
                    }

                    drawCmds[drawId] = drawCmd;
                    gpuDrawInfos[drawId] = di;
                }
            }
        }
    };

    extern ConVar r_shadowmapRes;

    void StandardPipeline::setupTechnique(AssetID fragShaderID, AssetID vertShaderID)
    {
        AssetID standardFragID = AssetDB::pathToId("Shaders/standard.frag.spv");
        AssetID standardVertID = AssetDB::pathToId("Shaders/standard.vert.spv");

        // Based on https://stackoverflow.com/a/2595226
        uint32_t key = vertShaderID;
        key ^= fragShaderID + 0x9e3779b9 + (key << 6) + (key >> 2);

        VK::VertexBinding vb;
        vb.Size = sizeof(Vertex);
        vb.Binding = 0;
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
        vb.Attributes.emplace_back(1, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, normal));
        vb.Attributes.emplace_back(2, VK::TextureFormat::R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, tangent));
        vb.Attributes.emplace_back(3, VK::TextureFormat::R32_SFLOAT, (uint32_t)offsetof(Vertex, bitangentSign));
        vb.Attributes.emplace_back(4, VK::TextureFormat::R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));

        AssetID depthFS = AssetDB::pathToId("Shaders/standard_empty.frag.spv");
        AssetID depthAlphaTestFS = AssetDB::pathToId("Shaders/standard_alpha_test.frag.spv");

        VK::ShaderModule& mainFragModule = ShaderCache::getModule(fragShaderID);
        VK::ShaderModule& mainVertModule = ShaderCache::getModule(vertShaderID);
        VK::ShaderModule& depthFragModule = ShaderCache::getModule(depthFS);
        VK::ShaderModule& depthAlphaTestModule = ShaderCache::getModule(depthAlphaTestFS);

        uint32_t techniqueId = techniqueManager->createTechnique();
        VK::Core* core = ((VKRenderer*)engineInterfaces.renderer)->getCore();

        VK::PipelineBuilder pb{core};
        setupMainPassPipeline(pb, vb);
        pb.AddShader(VK::ShaderStage::Vertex, mainVertModule)
          .AddShader(VK::ShaderStage::Fragment, mainFragModule);

        techniqueManager->registerVariant(techniqueId, pb.Build(), VariantFlags::None);
        techniqueManager->registerVariant(techniqueId, pb.Build(), VariantFlags::AlphaTest);

        VK::PipelineBuilder pb2{core};
        setupDepthPassPipeline(pb2, vb);
        pb2.AddShader(VK::ShaderStage::Vertex, mainVertModule)
           .AddShader(VK::ShaderStage::Fragment, fragShaderID == standardFragID ? depthFragModule : mainFragModule);

        techniqueManager->registerVariant(techniqueId, pb2.Build(), VariantFlags::DepthPrepass);

        VK::PipelineBuilder pb3{core};
        setupDepthPassPipeline(pb3, vb);
        pb3.AddShader(VK::ShaderStage::Vertex, mainVertModule)
           .AddShader(VK::ShaderStage::Fragment, fragShaderID == standardFragID ? depthAlphaTestModule : mainFragModule)
           .AlphaToCoverage(true);
        techniqueManager->registerVariant(techniqueId, pb3.Build(),
                                          VariantFlags::DepthPrepass | VariantFlags::AlphaTest);

        customShaderTechniques.insert({key, techniqueId});
    }

#define GPU_BEGIN(timestamp) timestampPool->WriteTimestamp(cb, timestampOffset + (timestamp * 2) + 0)
#define GPU_END(timestamp) timestampPool->WriteTimestamp(cb, timestampOffset + (timestamp * 2) + 1)

    void StandardPipeline::draw(entt::registry& reg, R2::VK::CommandBuffer& cb)
    {
        ZoneScoped;
        VKRenderer* renderer = (VKRenderer*)engineInterfaces.renderer;
        VK::Core* core = renderer->getCore();
        RenderMeshManager* meshManager = renderer->getMeshManager();
        BindlessTextureManager* btm = renderer->getBindlessTextureManager();
        VKTextureManager* textureManager = renderer->getTextureManager();
        uint32_t frameIdx = renderer->getCore()->GetFrameIndex();

        // Load necessary material techniques
        reg.view<WorldObject>().each([&](WorldObject& wo)
        {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++)
            {
                if (!wo.presentMaterials[i]) continue;
                AssetID material = wo.materials[i];
                if (!RenderMaterialManager::IsMaterialLoaded(material)) continue;
                const MaterialInfo& info = RenderMaterialManager::GetMaterialInfo(material);

                if (info.fragmentShader != INVALID_ASSET || info.vertexShader != INVALID_ASSET)
                {
                    AssetID fragShaderID = info.fragmentShader;
                    AssetID vertShaderID = info.vertexShader;
                    AssetID standardFragID = AssetDB::pathToId("Shaders/standard.frag.spv");
                    AssetID standardVertID = AssetDB::pathToId("Shaders/standard.vert.spv");

                    if (fragShaderID == INVALID_ASSET)
                        fragShaderID = standardFragID;

                    if (vertShaderID == INVALID_ASSET)
                        vertShaderID = standardVertID;

                    // Based on https://stackoverflow.com/a/2595226
                    uint32_t key = vertShaderID;
                    key ^= fragShaderID + 0x9e3779b9 + (key << 6) + (key >> 2);

                    if (customShaderTechniques.contains(key)) continue;
                    setupTechnique(fragShaderID, vertShaderID);
                }
            }
        });

        reg.view<SkinnedWorldObject>().each([&](SkinnedWorldObject& wo)
        {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++)
            {
                if (!wo.presentMaterials[i]) continue;
                AssetID material = wo.materials[i];
                if (!RenderMaterialManager::IsMaterialLoaded(material)) continue;
                const MaterialInfo& info = RenderMaterialManager::GetMaterialInfo(material);

                if (info.fragmentShader != INVALID_ASSET || info.vertexShader != INVALID_ASSET)
                {
                    AssetID fragShaderID = info.fragmentShader;
                    AssetID vertShaderID = info.vertexShader;
                    AssetID standardFragID = AssetDB::pathToId("Shaders/standard.frag.spv");
                    AssetID standardVertID = AssetDB::pathToId("Shaders/standard.vert.spv");

                    if (fragShaderID == INVALID_ASSET)
                        fragShaderID = standardFragID;

                    if (vertShaderID == INVALID_ASSET)
                        vertShaderID = standardVertID;

                    // Based on https://stackoverflow.com/a/2595226
                    uint32_t key = vertShaderID;
                    key ^= fragShaderID + 0x9e3779b9 + (key << 6) + (key >> 2);

                    if (customShaderTechniques.contains(key)) continue;
                    setupTechnique(fragShaderID, vertShaderID);
                }
            }
        });

        // retrieve timings from last frame
        uint64_t retrievedTimestamps[NUM_TIMESTAMPS];
        if (timestampPool->GetTimestamps(core->GetFrameIndex() * NUM_TIMESTAMPS, NUM_TIMESTAMPS, retrievedTimestamps))
        {
            double times[TS_Count];
            for (int i = 0; i < TS_Count; i++)
            {
                int ts = i * 2;
                double timeTaken = (double)(retrievedTimestamps[ts + 1] - retrievedTimestamps[ts]);
                timeTaken *= core->GetDeviceInfo().TimestampPeriod;
                times[i] = timeTaken;
            }

            renderer->getDebugStats().skinningTime = times[TS_Skinning];
            renderer->getDebugStats().depthPassTime = times[TS_DepthPrepass];
            renderer->getDebugStats().lightCullTime = times[TS_LightCull];
            renderer->getDebugStats().mainPassTime = times[TS_MainPass];
            renderer->getDebugStats().bloomTime = times[TS_Bloom];
            renderer->getDebugStats().tonemapTime = times[TS_Tonemap];
        }
        timestampPool->Reset(cb, core->GetFrameIndex() * NUM_TIMESTAMPS, NUM_TIMESTAMPS);
        int timestampOffset = core->GetFrameIndex() * NUM_TIMESTAMPS;

        SceneGlobals* globals = (SceneGlobals*)sceneGlobals->Map();
        globals->time = renderer->getTime();
        globals->shadowmapResolution = r_shadowmapRes.getInt();
        sceneGlobals->Unmap();

        // If there's anything in the convolution queue, convolute 1 cubemap
        // per frame (convolution is slow!)
        AssetID convoluteID;
        while (convoluteQueue.try_dequeue(convoluteID))
        {
            cb.BeginDebugLabel("Cubemap Convolution", 0.1f, 0.1f, 0.1f);

            if (!textureManager->isLoaded(convoluteID))
                fatalErr("Can't convolute an unloaded cubemap");
            uint32_t convoluteHandle = textureManager->get(convoluteID);
            VK::Texture* tex = btm->GetTextureAt(convoluteHandle);
            cubemapConvoluter->Convolute(cb, tex);

            cb.EndDebugLabel();
        }

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

            new(&frustums[i]) Frustum();
            frustums[i].fromVPMatrix(proj * view);
        }

        core->QueueBufferUpload(multiVPBuffer.Get(), &multiVPs, sizeof(multiVPs), 0);

        TasksFinished finisher;

        LightUB* lightUB = (LightUB*)lightBuffers->MapCurrent();

        FillLightBufferTask fillTask{lightUB, reg, textureManager};
        fillTask.shadowmapManager = renderer->getShadowmapManager();
        fillTask.numViews = rttPass->getSettings().numViews;
        fillTask.frustums = frustums;
        fillTask.dbgStats = &renderer->getDebugStats();

        AllocatedSkinnedStorageTask allocSkinnedStorageTask{renderer, reg};
        allocSkinnedStorageTask.m_SetSize = reg.view<SkinnedWorldObject>().size();
        allocSkinnedStorageTask.m_MinRange = 10;

        finisher.SetDependenciesVec<std::vector<enki::Dependency>, enki::ITaskSet>(
            finisher.dependencies, {&fillTask, &allocSkinnedStorageTask});

        g_taskSched.AddTaskSetToPipe(&fillTask);
        g_taskSched.AddTaskSetToPipe(&allocSkinnedStorageTask);
        g_taskSched.WaitforTask(&finisher);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderStorageRead, VK::PipelineStageFlags::FragmentShader);
        modelMatrixBuffers->GetCurrentBuffer()->Acquire(
            cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::VertexShader);


        GPU_BEGIN(TS_Skinning);
        computeSkinner->Execute(cb, reg);
        GPU_END(TS_Skinning);

        glm::mat4* modelMatricesMapped = (glm::mat4*)modelMatrixBuffers->MapCurrent();
        GPUDrawInfo* drawInfosMapped = (GPUDrawInfo*)drawInfoBuffers->MapCurrent();

        AtomicBufferWrapper<glm::mat4> matrixWrapper{modelMatricesMapped};

        FillDrawBufferTask fdbTask{renderer, reg};
        fdbTask.numViews = rttPass->getSettings().numViews;
        fdbTask.frustums = frustums;
        fdbTask.modelMatrices = &matrixWrapper;
        fdbTask.gpuDrawInfos = drawInfosMapped;
        fdbTask.drawCmds = drawCmds.data();
        fdbTask.onlyStatics = rttPass->getSettings().staticsOnly;
        fdbTask.customShaderTechniques = &customShaderTechniques;

        fdbTask.m_SetSize = reg.view<WorldObject>().size();

        FillDrawBufferSkinnedTask fdbsTask{renderer, reg, fdbTask.drawIdCounter};
        fdbsTask.numViews = rttPass->getSettings().numViews;
        fdbsTask.frustums = frustums;
        fdbsTask.modelMatrices = &matrixWrapper;
        fdbsTask.gpuDrawInfos = drawInfosMapped;
        fdbsTask.drawCmds = drawCmds.data();
        fdbsTask.onlyStatics = rttPass->getSettings().staticsOnly;
        fdbsTask.customShaderTechniques = &customShaderTechniques;

        fdbsTask.m_SetSize = reg.view<SkinnedWorldObject>().size();

        TasksFinished finisher2;
        finisher2.SetDependenciesVec<std::vector<enki::Dependency>, enki::ITaskSet>(
            finisher2.dependencies, {&fdbTask, &fdbsTask});

        g_taskSched.AddTaskSetToPipe(&fdbTask);
        g_taskSched.AddTaskSetToPipe(&fdbsTask);
        g_taskSched.WaitforTask(&finisher2);

        modelMatrixBuffers->UnmapCurrent();
        drawInfoBuffers->UnmapCurrent();

        releaseAssert(fdbTask.drawIdCounter < drawCmds.size());

        // Depth Pre-Pass
        cb.BeginDebugLabel("Depth Pre-Pass", 0.1f, 0.1f, 0.1f);
        GPU_BEGIN(TS_DepthPrepass);

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

        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), descriptorSets[core->GetFrameIndex()].Get(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), &btm->GetTextureDescriptorSet(), 1);
        cb.BindVertexBuffer(0, meshManager->getVertexBuffer(), 0);
        cb.BindIndexBuffer(meshManager->getIndexBuffer(), 0, VK::IndexType::Uint32);

        uint32_t lastTechniqueIdx = ~0u;
        VariantFlags lastVariantFlags = VariantFlags::None;
        for (int i = 0; i < fdbTask.drawIdCounter; i++)
        {
            const StandardDrawCommand& drawCmd = drawCmds[i];
            if (drawCmd.techniqueIdx != lastTechniqueIdx || drawCmd.variantFlags != lastVariantFlags)
            {
                // change pipeline to depth pre-pass pipeline for this technique
                VariantFlags finalFlags = drawCmd.variantFlags | VariantFlags::DepthPrepass;
                cb.BindPipeline(techniqueManager->getPipelineVariant(drawCmd.techniqueIdx, finalFlags));
                lastTechniqueIdx = drawCmd.techniqueIdx;
                lastVariantFlags = lastVariantFlags;
            }
            cb.DrawIndexed(drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, i);
        }
        depthPass.End(cb);
        renderer->getDebugStats().numDrawCalls = fdbTask.drawIdCounter;
        GPU_END(TS_DepthPrepass);

        cb.EndDebugLabel();

        // Run light culling using the depth buffer
        GPU_BEGIN(TS_LightCull);
        lightCull->Execute(cb);
        GPU_END(TS_LightCull);

        lightTileBuffer->Acquire(cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::FragmentShader);

        // Actual "opaque" pass
        cb.BeginDebugLabel("Opaque Pass", 0.5f, 0.1f, 0.1f);
        GPU_BEGIN(TS_MainPass);

        VK::RenderPass colorPass;
        colorPass.ColorAttachment(colorBuffer.Get(), VK::LoadOp::Clear, VK::StoreOp::Store)
                 .ColorAttachmentClearValue(VK::ClearValue::FloatColorClear(0.0f, 0.0f, 0.0f, 1.0f))
                 .DepthAttachment(depthBuffer.Get(), VK::LoadOp::Load, VK::StoreOp::Store)
                 .RenderArea(rttPass->width, rttPass->height)
                 .ViewMask(getViewMask(rttPass->getSettings().numViews));

        colorPass.Begin(cb);

        lastTechniqueIdx = ~0u;
        lastVariantFlags = VariantFlags::None;
        for (int i = 0; i < fdbTask.drawIdCounter; i++)
        {
            const StandardDrawCommand& drawCmd = drawCmds[i];
            if (drawCmd.techniqueIdx != lastTechniqueIdx || drawCmd.variantFlags != lastVariantFlags)
            {
                // change pipeline to main pipeline for this technique
                VariantFlags finalFlags = drawCmd.variantFlags;
                cb.BindPipeline(techniqueManager->getPipelineVariant(drawCmd.techniqueIdx, finalFlags));
                lastTechniqueIdx = drawCmd.techniqueIdx;
                lastVariantFlags = lastVariantFlags;
            }
            cb.DrawIndexed(drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, i);
        }

        skyboxRenderer->Execute(cb);

        cb.BeginDebugLabel("Debug Lines", 0.1f, 0.1f, 0.1f);
        size_t dbgLinesCount;
        const DebugLine* dbgLines = renderer->getCurrentDebugLines(&dbgLinesCount);

        debugLineDrawer->Execute(cb, dbgLines, dbgLinesCount);
        cb.EndDebugLabel();

        particleRenderer->Execute(cb, reg);

        colorPass.End(cb);
        cb.EndDebugLabel();
        GPU_END(TS_MainPass);

        // Post-processing
        static ConVar r_skipBloom{"r_skipBloom", "0", "Skips bloom rendering."};
        GPU_BEGIN(TS_Bloom);
        if (!r_skipBloom)
            bloom->Execute(cb);
        GPU_END(TS_Bloom);

        GPU_BEGIN(TS_Tonemap);
        tonemapper->Execute(cb, r_skipBloom);
        GPU_END(TS_Tonemap);

        lightBuffers->UnmapCurrent();
    }

    void StandardPipeline::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        overrideViews[viewIndex] = viewMatrix;
        overrideProjs[viewIndex] = projectionMatrix;
    }

    VK::Texture* StandardPipeline::getHDRTexture()
    {
        return colorBuffer.Get();
    }
}
