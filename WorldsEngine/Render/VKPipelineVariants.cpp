#include "RenderInternal.hpp"
#include <Render/ShaderCache.hpp>
#include <Core/ConVar.hpp>
#include <Util/EnumUtil.hpp>
#include <Render/ShaderReflector.hpp>

namespace worlds {
    ConVar enableParallaxMapping("r_doParallaxMapping", "0");
    ConVar maxParallaxLayers("r_maxParallaxLayers", "32");
    ConVar minParallaxLayers("r_minParallaxLayers", "4");
    ConVar enableProxyAO("r_enableProxyAO", "1");
    ConVar enableDepthPrepass("r_depthPrepass", "1");

    struct StandardSpecConsts {
        VkBool32 enablePicking = false;
        float parallaxMaxLayers = 32.0f;
        float parallaxMinLayers = 4.0f;
        VkBool32 doParallax = false;
        VkBool32 enableProxyAO = false;
    };

    void setupVertexFormat(const VertexAttributeBindings& vab, vku::PipelineMaker& pm) {
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));

        if (vab.position != -1)
            pm.vertexAttribute(vab.position, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));

        if (vab.normal != -1)
            pm.vertexAttribute(vab.normal, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, normal));

        if (vab.tangent != -1)
            pm.vertexAttribute(vab.tangent, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, tangent));

        if (vab.bitangentSign != -1)
            pm.vertexAttribute(vab.bitangentSign, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(Vertex, bitangentSign));

        if (vab.uv != -1)
            pm.vertexAttribute(vab.uv, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
    }

    void setupSkinningVertexFormat(const VertexAttributeBindings& vab, vku::PipelineMaker& pm) {
        pm.vertexBinding(1, (uint32_t)sizeof(VertSkinningInfo));
        if (vab.boneWeights != -1)
            pm.vertexAttribute(vab.boneWeights, 1, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(VertSkinningInfo, weights));
        
        if (vab.boneIds != -1)
            pm.vertexAttribute(vab.boneIds, 1, VK_FORMAT_R32G32B32A32_UINT, (uint32_t)offsetof(VertSkinningInfo, boneIds));
    }

    class StandardPipelineMaker {
    public:
        StandardPipelineMaker(AssetID vertexShader, AssetID fragmentShader) {
            vs = vertexShader;
            fs = fragmentShader;
        }

        StandardPipelineMaker& msaaSamples(int val) {
            _msaaSamples = val;
            return *this;
        }

        StandardPipelineMaker& setPickingEnabled(bool val) {
            enablePicking = val;
            return *this;
        }

        StandardPipelineMaker& setCullMode(VkCullModeFlags val) {
            cullFlags = val;
            return *this;
        }

        StandardPipelineMaker& useSpecConstants(bool val) {
            _useSpecConstants = val;
            return *this;
        }

        StandardPipelineMaker& setEnableSkinning(bool val) {
            useSkinningAttributes = val;
            return *this;
        }

        vku::Pipeline createPipeline(VulkanHandles* handles, VkPipelineLayout layout, VkRenderPass renderPass) {
            VkSpecializationMapEntry entries[5] = {
                { 0, offsetof(StandardSpecConsts, enablePicking),     sizeof(VkBool32) },
                { 1, offsetof(StandardSpecConsts, parallaxMaxLayers), sizeof(float) },
                { 2, offsetof(StandardSpecConsts, parallaxMinLayers), sizeof(float) },
                { 3, offsetof(StandardSpecConsts, doParallax),        sizeof(VkBool32) },
                { 4, offsetof(StandardSpecConsts, enableProxyAO),     sizeof(VkBool32) }
            };

            VkSpecializationInfo standardSpecInfo{ 5, entries, sizeof(StandardSpecConsts) };

            vku::PipelineMaker pm{ 1600, 900 };
            VkShaderModule fragmentShader = ShaderCache::getModule(handles->device, fs);
            VkShaderModule vertexShader = ShaderCache::getModule(handles->device, vs);
            ShaderReflector sr {vs};
            VertexAttributeBindings vab = sr.getVertexAttributeBindings();

            StandardSpecConsts spc{
                enablePicking,
                maxParallaxLayers.getFloat(),
                minParallaxLayers.getFloat(),
                (bool)enableParallaxMapping.getInt(),
                (bool)enableProxyAO.getInt()
            };

            standardSpecInfo.pData = &spc;

            if (_useSpecConstants)
                pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main", &standardSpecInfo);
            else
                pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);

            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
            setupVertexFormat(vab, pm);

            if (useSkinningAttributes)
                setupSkinningVertexFormat(vab, pm);

            pm.cullMode(cullFlags);

            if ((int)enableDepthPrepass)
                pm.depthWriteEnable(false)
                .depthTestEnable(true)
                .depthCompareOp(VK_COMPARE_OP_EQUAL);
            else
                pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);

            pm.blendBegin(false);
            pm.frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);

            pm.rasterizationSamples(vku::sampleCountFlags(_msaaSamples));

            pm.dynamicState(VK_DYNAMIC_STATE_VIEWPORT);
            pm.dynamicState(VK_DYNAMIC_STATE_SCISSOR);

            if (handles->hasOutOfOrderRasterization)
                pm.rasterizationOrderAMD(VK_RASTERIZATION_ORDER_RELAXED_AMD);

            return pm.create(handles->device, handles->pipelineCache, layout, renderPass);
        }
    private:
        AssetID vs;
        AssetID fs;
        int _msaaSamples = 1;
        bool enablePicking = false;
        bool useSkinningAttributes = false;
        bool _useSpecConstants = false;
        VkCullModeFlags cullFlags = VK_CULL_MODE_BACK_BIT;
    };

    class DepthPrepassPipelineMaker {
    public:
        DepthPrepassPipelineMaker(AssetID vs, AssetID fs)
            : vs(vs)
            , fs(fs) {
        }

        DepthPrepassPipelineMaker& setEnableSkinning(bool enable) {
            enableSkinning = enable;
            return *this;
        }

        DepthPrepassPipelineMaker& setEnableAlphaTest(bool enable) {
            enableAlphaTest = enable;
            return *this;
        }

        DepthPrepassPipelineMaker& setMsaaSamples(int samples) {
            msaaSamples = samples;
            return *this;
        }

        vku::Pipeline createPipeline(VulkanHandles* handles, VkPipelineLayout layout, VkRenderPass renderPass) {
            vku::PipelineMaker pm { 1600, 900 };

            pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderCache::getModule(handles->device, fs));
            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, ShaderCache::getModule(handles->device, vs));
            pm.cullMode(VK_CULL_MODE_BACK_BIT);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
            pm.blendBegin(false);
            pm.frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);

            pm.rasterizationSamples(vku::sampleCountFlags(msaaSamples));
            pm.alphaToCoverageEnable(enableAlphaTest);
            pm.subPass(0);

            if (handles->hasOutOfOrderRasterization)
                pm.rasterizationOrderAMD(VK_RASTERIZATION_ORDER_RELAXED_AMD);

            pm.dynamicState(VK_DYNAMIC_STATE_VIEWPORT).dynamicState(VK_DYNAMIC_STATE_SCISSOR);

            ShaderReflector vsr{vs};
            VertexAttributeBindings vab = vsr.getVertexAttributeBindings();

            //pm.vertexBinding(0, (uint32_t)sizeof(Vertex));

            //pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
            //pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
            setupVertexFormat(vab, pm);

            if (enableSkinning) {
                //pm.vertexBinding(1, (uint32_t)sizeof(VertSkinningInfo));
                //pm.vertexAttribute(2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(VertSkinningInfo, weights));
                //pm.vertexAttribute(3, 1, VK_FORMAT_R32G32B32A32_UINT, (uint32_t)offsetof(VertSkinningInfo, boneIds));
                setupSkinningVertexFormat(vab, pm);
            }

            return pm.create(handles->device, handles->pipelineCache, layout, renderPass);
        }
    private:
        AssetID vs;
        AssetID fs;
        bool enableSkinning = false;
        bool enableAlphaTest = false;
        int msaaSamples = 1;
    };

    VKPipelineVariants::VKPipelineVariants(VulkanHandles* handles, bool depthOnly, VkPipelineLayout layout, VkRenderPass renderPass) 
        : handles(handles)
        , depthOnly(depthOnly)
        , layout(layout)
        , renderPass(renderPass) {
    }

    VkPipeline VKPipelineVariants::getPipeline(bool enablePicking, int msaaSamples, ShaderVariantFlags flags) {
        ShaderKey key {
            enablePicking, msaaSamples,
            flags
        };

        uint32_t hash = key.hash();
        if (cache.contains(hash)) {
            return cache.at(hash);
        }

        if (depthOnly) {
            DepthPrepassPipelineMaker pm { getVS(flags), getFS(flags) };
            pm.setEnableAlphaTest(enumHasFlag(flags, ShaderVariantFlags::AlphaTest))
              .setEnableSkinning(enumHasFlag(flags, ShaderVariantFlags::Skinnning))
              .setMsaaSamples(msaaSamples);

            vku::Pipeline p = pm.createPipeline(handles, layout, renderPass);
            VkPipeline safe = p;
            cache.insert({ hash, std::move(p) });
            return safe;
        } else {
            StandardPipelineMaker pm { getVS(flags), getFS(flags) };
            pm.setCullMode(enumHasFlag(flags, ShaderVariantFlags::NoBackfaceCulling) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT)
              .msaaSamples(msaaSamples)
              .setPickingEnabled(enablePicking)
              .setEnableSkinning(enumHasFlag(flags, ShaderVariantFlags::Skinnning));

            vku::Pipeline p = pm.createPipeline(handles, layout, renderPass);
            VkPipeline safe = p;
            cache.insert({ hash, std::move(p) });
            return safe;
        }
    }

    AssetID VKPipelineVariants::getFS(ShaderVariantFlags flags) {
        if (depthOnly) {
            if (enumHasFlag(flags, ShaderVariantFlags::AlphaTest)) {
                return AssetDB::pathToId("Shaders/alpha_test_prepass.frag.spv");
            } else {
                return AssetDB::pathToId("Shaders/blank.frag.spv");
            }
        }

        return AssetDB::pathToId("Shaders/standard.frag.spv");
    }

    AssetID VKPipelineVariants::getVS(ShaderVariantFlags flags) {
        if (depthOnly) {
            if (enumHasFlag(flags, ShaderVariantFlags::Skinnning)) {
                return AssetDB::pathToId("Shaders/depth_prepass_skinned.vert.spv");
            } else {
                return AssetDB::pathToId("Shaders/depth_prepass.vert.spv");
            }
        }

        if (enumHasFlag(flags, ShaderVariantFlags::Skinnning)) {
            return AssetDB::pathToId("Shaders/standard_skinned.vert.spv");
        } else {
            return AssetDB::pathToId("Shaders/standard.vert.spv");
        }
    }

    uint32_t VKPipelineVariants::ShaderKey::hash() {
        uint32_t h = 
            (uint32_t)enablePicking
          | (((uint32_t)msaaSamples) << 1)
          | (((uint32_t)flags) << 8);

        return h;
    }
}