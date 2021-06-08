#include "RenderPasses.hpp"
#include "ShaderCache.hpp"
#include "Loaders/TextureLoader.hpp"
#include <robin_hood.h>
#include <nlohmann/json.hpp>
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_stdlib.h"
#include "glm/gtc/type_ptr.hpp"
#include "../UI/WorldTextComponent.hpp"

namespace worlds {
    struct FontChar {
        uint32_t codepoint;

        uint16_t x;
        uint16_t y;

        uint16_t width;
        uint16_t height;

        int16_t originX;
        int16_t originY;

        uint16_t advance;
    };
    robin_hood::unordered_flat_map<uint32_t, FontChar> fontCharacters;
    float fontWidth;
    float fontHeight;

    struct UIVertex {
        glm::vec3 pos;
        glm::vec2 uv;
    };

    WorldSpaceUIPass::WorldSpaceUIPass(VulkanHandles* handles) : handles {handles} {
        if (fontCharacters.size() == 0) {
            const char* fontPath = "UI/SDFFonts/mulish.json";
            PHYSFS_File* f = PHYSFS_openRead(fontPath);

            if (f == nullptr) {
                auto err = PHYSFS_getLastErrorCode();
                auto errStr = PHYSFS_getErrorByCode(err);
                logErr(WELogCategoryUI, "Failed to open %s: %s", fontPath, errStr);
            }

            size_t fileSize = PHYSFS_fileLength(f);
            std::string str;
            str.resize(fileSize);
            // Add a null byte to the end to make a C string
            PHYSFS_readBytes(f, str.data(), fileSize);
            PHYSFS_close(f);

            try {
                auto j = nlohmann::json::parse(str);
                fontWidth = j["width"];
                fontHeight = j["height"];

                auto& chars = j["characters"];
                for (auto& charPair : chars.items()) {
                    auto& charVal = charPair.value();

                    // TODO: Handle Unicode correctly!
                    // THIS WILL BREAK FOR NON-ASCII CHARACTERS!!!!!
                    uint32_t codepoint = charPair.key()[0];
                    FontChar thisChar {
                        .codepoint = codepoint,
                        .x = charVal["x"],
                        .y = charVal["y"],
                        .width = charVal["width"],
                        .height = charVal["height"],
                        .originX = charVal["originX"],
                        .originY = charVal["originY"],
                        .advance = charVal["advance"]
                    };

                    fontCharacters.insert({ codepoint, thisChar });
                }
            } catch(nlohmann::detail::exception& ex) {
                logErr("Failed to parse font");
            }
        }
    }

    void buildCharQuad(glm::vec3 center, float scale, const FontChar& character, UIVertex* verts) {
        center /= scale;
        // 0 ------- 1
        // |\        |
        // | \       |
        // |  \      |
        // |   \     |
        // |    \    |
        // |     \   |
        // |      \  |
        // |       \ |
        // 2 ------- 3
        // to
        // 0 ------- 1
        // |\        |
        // | \       |
        // |  \      |
        // |   \     |
        // |    \    |
        // |     \   |
        // |      \  |
        // |       \ |
        // 3 ------- 2

        // magic number is -240

        float x0 = center.x - character.originX;
        float y0 = center.y - character.originY;
        float s0 = character.x / fontWidth;
        float t0 = character.y / fontHeight;

        float x1 = center.x - character.originX + character.width;
        float y1 = center.y - character.originY;
        float s1 = (character.x + character.width) / fontWidth;
        float t1 = character.y / fontHeight;

        float x2 = center.x - character.originX;
        float y2 = center.y - character.originY + character.height;
        float s2 = character.x / fontWidth;
        float t2 = (character.y + character.height) / fontHeight;

        float x3 = center.x - character.originX + character.width;
        float y3 = center.y - character.originY + character.height;
        float s3 = (character.x + character.width) / fontWidth;
        float t3 = (character.y + character.height) / fontHeight;

        verts[0] = UIVertex {
            .pos = { x0 * scale, y0 * -scale, center.z },
            .uv = { s0, t0 }
        };

        verts[1] = UIVertex {
            .pos = { x1 * scale, y1 * -scale, center.z },
            .uv = { s1, t1 }
        };

        verts[2] = UIVertex {
            .pos = { x3 * scale, y3 * -scale, center.z },
            .uv = { s3, t3 }
        };

        verts[3] = UIVertex {
            .pos = { x2 * scale, y2 * -scale, center.z },
            .uv = { s2, t2 }
        };
    }

    void WorldSpaceUIPass::setup(RenderContext& ctx, vk::RenderPass renderPass, vk::DescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
        descriptorSetLayout = dslm.createUnique(handles->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
        plm.descriptorSetLayout(*descriptorSetLayout);
        pipelineLayout = plm.createUnique(handles->device);

        vku::PipelineMaker pm{ctx.passWidth, ctx.passHeight};
        pm.depthTestEnable(true);
        pm.depthWriteEnable(false);
        pm.depthCompareOp(vk::CompareOp::eGreater);

        pm.shader(vk::ShaderStageFlagBits::eVertex, ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/world_ui.vert.spv")));
        pm.shader(vk::ShaderStageFlagBits::eFragment, ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/world_ui.frag.spv")));
        pm.vertexBinding(0, (uint32_t)sizeof(UIVertex));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(UIVertex, pos));
        pm.vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(UIVertex, uv));
        pm.cullMode(vk::CullModeFlagBits::eNone);
        pm.blendBegin(true);
        pm.subPass(1);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = vku::sampleCountFlags(handles->graphicsSettings.msaaLevel);
        pm.multisampleState(pmsci);

        textPipeline = pm.createUnique(handles->device, handles->pipelineCache, *pipelineLayout, renderPass);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*descriptorSetLayout);
        descriptorSet = std::move(dsm.createUnique(handles->device, handles->descriptorPool)[0]);

        TextureData sdfData = loadTexData(AssetDB::pathToId("UI/SDFFonts/mulish.png"));
        textSdf = uploadTextureVk(*handles, sdfData);
        std::free(sdfData.data);

        vku::SamplerMaker sm;
        sm.minFilter(vk::Filter::eLinear).magFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).anisotropyEnable(true).maxAnisotropy(16.0f).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        sampler = sm.createUnique(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*descriptorSet);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));
        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, textSdf.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        dsu.update(handles->device);
    }

    void WorldSpaceUIPass::updateBuffers(entt::registry& reg) {
        size_t requiredBufferCapacity = 0;
        auto textView = reg.view<WorldTextComponent>();

        textView.each([&](WorldTextComponent& wtc) {
            requiredBufferCapacity += wtc.text.size();
        });

        if (bufferCapacity < requiredBufferCapacity) {
            if (vb.buffer()) {
                vb.destroy();
                ib.destroy();
            }

            vb = vku::GenericBuffer{
                handles->device,
                handles->allocator,
                vk::BufferUsageFlagBits::eVertexBuffer,
                sizeof(UIVertex) * 4 * requiredBufferCapacity,
                VMA_MEMORY_USAGE_CPU_TO_GPU, "UI Vertex Buffer"
            };
            ib = vku::GenericBuffer{
                handles->device,
                handles->allocator,
                vk::BufferUsageFlagBits::eIndexBuffer,
                sizeof(uint32_t) * 6 * requiredBufferCapacity,
                VMA_MEMORY_USAGE_CPU_TO_GPU, "UI Index Buffer"
            };
        }

        if (!vb.buffer()) return;

        UIVertex* vbMap = (UIVertex*)vb.map(handles->device);
        uint32_t* ibMap = (uint32_t*)ib.map(handles->device);
        int vertexOffset = 0;
        uint32_t idxOffset = 0;
        reg.view<WorldTextComponent>().each([&](WorldTextComponent& wtc) {
            if (wtc.text.size() == 0) return;

            float totalWidth = 0.0f;
            for (auto c : wtc.text) {
                totalWidth += fontCharacters.at(c).advance;
            }

            wtc.idxOffset = idxOffset;
            float xPos = 0.0f;

            if (wtc.hAlign == HTextAlign::Middle)
                xPos = -totalWidth * 0.5f;
            else if (wtc.hAlign == HTextAlign::Right)
                xPos = -totalWidth;

            xPos *= wtc.textScale;

            for (auto c : wtc.text) {
                buildCharQuad(glm::vec3{xPos, 0.0f, 0.0f}, wtc.textScale, fontCharacters.at(c), vbMap);
                vbMap += 4;
                xPos += fontCharacters.at(c).advance * wtc.textScale;

                const uint32_t indexPattern[] = {
                    0, 1, 2,
                    0, 3, 2
                };

                for (int i = 0; i < 6; i++) {
                    *ibMap = indexPattern[i] + vertexOffset;
                    ibMap++;
                }
                idxOffset += 6;
                vertexOffset += 4;
            }
        });
        vb.unmap(handles->device);
        ib.unmap(handles->device);
    }

    void WorldSpaceUIPass::prePass(RenderContext& ctx) {
        updateBuffers(ctx.registry);
    }

    void WorldSpaceUIPass::execute(RenderContext& ctx) {
        auto cmdBuf = ctx.cmdBuf;
        if (vb.buffer()) {
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *textPipeline);
            cmdBuf.bindVertexBuffers(0, vb.buffer(), vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(ib.buffer(), 0, vk::IndexType::eUint32);
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSet, nullptr);
            ctx.registry.view<WorldTextComponent, Transform>().each([&](WorldTextComponent& wtc, Transform& tf) {
                glm::mat4 model = tf.getMatrix();
                cmdBuf.pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, model);
                cmdBuf.drawIndexed(wtc.text.size() * 6, 1, wtc.idxOffset, 0, 0);
            });
        }
    }
}
