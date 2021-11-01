#include "RenderPasses.hpp"
#include "ShaderCache.hpp"
#include "Loaders/TextureLoader.hpp"
#include <robin_hood.h>
#include <nlohmann/json.hpp>
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_stdlib.h"
#include "glm/gtc/type_ptr.hpp"
#include "../UI/WorldTextComponent.hpp"
#include "vku/DescriptorSetUtil.hpp"

namespace worlds {
    struct UIVertex {
        glm::vec3 pos;
        glm::vec2 uv;
    };

    struct FontShaderPushConstants {
        glm::mat4 model;
        uint32_t textureIdx;
        glm::vec3 pad;
    };

    robin_hood::unordered_flat_map<AssetID, SDFFont> fonts;

    SDFFont& WorldSpaceUIPass::getFont(AssetID id) {
        if (id == INVALID_ASSET) return fonts.at(AssetDB::pathToId("UI/SDFFonts/mulish.json"));
        return fonts.at(id);
    }

    void WorldSpaceUIPass::loadFont(AssetID id) {
        if (fonts.contains(id)) return;
        SDFFont font;
        PHYSFS_File* f = AssetDB::openAssetFileRead(id);

        if (f == nullptr) {
            auto err = PHYSFS_getLastErrorCode();
            auto errStr = PHYSFS_getErrorByCode(err);
            std::string path = AssetDB::idToPath(id);
            logErr(WELogCategoryUI, "Failed to open %s: %s", path.c_str(), errStr);
            return;
        }

        size_t fileSize = PHYSFS_fileLength(f);
        std::string str;
        str.resize(fileSize);
        // Add a null byte to the end to make a C string
        PHYSFS_readBytes(f, str.data(), fileSize);
        PHYSFS_close(f);

        try {
            auto j = nlohmann::json::parse(str);
            font.width = j["width"];
            font.height = j["height"];

            std::string atlasPath = "UI/SDFFonts/" + j["atlas"].get<std::string>();
            TextureData sdfData = loadTexData(AssetDB::pathToId(atlasPath));
            font.atlas = uploadTextureVk(*handles, sdfData);
            std::free(sdfData.data);

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

                font.characters.insert({ codepoint, thisChar });
            }
        } catch(nlohmann::detail::exception& ex) {
            logErr("Failed to parse font");
            return;
        }

        font.index = nextFontIdx;
        nextFontIdx++;

        fonts.insert({ id, std::move(font) });
    }

    WorldSpaceUIPass::WorldSpaceUIPass(VulkanHandles* handles) : handles {handles} {
        loadFont(AssetDB::pathToId("UI/SDFFonts/mulish.json"));
        loadFont(AssetDB::pathToId("UI/SDFFonts/potra.json"));
    }

    void buildCharQuad(glm::vec3 center, float scale, const FontChar& character, const SDFFont& font, UIVertex* verts) {
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

        float x0 = center.x - character.originX;
        float y0 = center.y - character.originY;
        float s0 = character.x / font.width;
        float t0 = character.y / font.height;

        float x1 = center.x - character.originX + character.width;
        float y1 = center.y - character.originY;
        float s1 = (character.x + character.width) / font.width;
        float t1 = character.y / font.height;

        float x2 = center.x - character.originX;
        float y2 = center.y - character.originY + character.height;
        float s2 = character.x / font.width;
        float t2 = (character.y + character.height) / font.height;

        float x3 = center.x - character.originX + character.width;
        float y3 = center.y - character.originY + character.height;
        float s3 = (character.x + character.width) / font.width;
        float t3 = (character.y + character.height) / font.height;

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

    void WorldSpaceUIPass::setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
        dslm.image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 8);
        dslm.bindFlag(1, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        descriptorSetLayout = dslm.create(handles->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FontShaderPushConstants));
        plm.descriptorSetLayout(descriptorSetLayout);
        pipelineLayout = plm.create(handles->device);

        vku::PipelineMaker pm{ctx.passWidth, ctx.passHeight};
        pm.depthTestEnable(true);
        pm.depthWriteEnable(false);
        pm.depthCompareOp(VK_COMPARE_OP_GREATER);

        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/world_ui.vert.spv")));
        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/world_ui.frag.spv")));
        pm.vertexBinding(0, (uint32_t)sizeof(UIVertex));
        pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(UIVertex, pos));
        pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(UIVertex, uv));
        pm.cullMode(VK_CULL_MODE_NONE);
        pm.blendBegin(true);

        pm.rasterizationSamples(vku::sampleCountFlags(ctx.passSettings.msaaSamples));

        textPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, renderPass);

        vku::DescriptorSetMaker dsm;
        dsm.layout(descriptorSetLayout);
        descriptorSet = std::move(dsm.create(handles->device, descriptorPool)[0]);


        vku::SamplerMaker sm;
        sm.minFilter(VK_FILTER_LINEAR).magFilter(VK_FILTER_LINEAR).mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR).anisotropyEnable(true).maxAnisotropy(16.0f).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        sampler = sm.create(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(descriptorSet);
        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

        for (const auto& fontPair : fonts) {
            const SDFFont& font = fontPair.second;
            dsu.beginImages(1, font.index, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, font.atlas.imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

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
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                sizeof(UIVertex) * 4 * requiredBufferCapacity,
                VMA_MEMORY_USAGE_CPU_TO_GPU, "UI Vertex Buffer"
            };
            ib = vku::GenericBuffer{
                handles->device,
                handles->allocator,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
            const auto& font = getFont(wtc.font);

            float totalWidth = 0.0f;
            for (auto c : wtc.text) {
                totalWidth += font.characters.at(c).advance;
            }

            wtc.idxOffset = idxOffset;
            float xPos = 0.0f;

            if (wtc.hAlign == HTextAlign::Middle)
                xPos = -totalWidth * 0.5f;
            else if (wtc.hAlign == HTextAlign::Right)
                xPos = -totalWidth;

            xPos *= wtc.textScale;

            for (auto c : wtc.text) {
                buildCharQuad(glm::vec3{xPos, 0.0f, 0.0f}, wtc.textScale, font.characters.at(c), font, vbMap);
                vbMap += 4;
                xPos += font.characters.at(c).advance * wtc.textScale;

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
        bool newFontLoaded = false;
        ctx.registry.view<WorldTextComponent>().each([&](WorldTextComponent& wtc) {
            if (!fonts.contains(wtc.font) && wtc.font != INVALID_ASSET) {
                loadFont(wtc.font);
                newFontLoaded = true;
            }
        });

        if (newFontLoaded) {
            vku::DescriptorSetUpdater dsu;
            dsu.beginDescriptorSet(descriptorSet);
            dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

            for (const auto& fontPair : fonts) {
                const SDFFont& font = fontPair.second;
                dsu.beginImages(1, font.index, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                dsu.image(sampler, font.atlas.imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            dsu.update(handles->device);
        }

        updateBuffers(ctx.registry);
    }

    void WorldSpaceUIPass::execute(RenderContext& ctx) {
        auto cmdBuf = ctx.cmdBuf;
        if (vb.buffer()) {
            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline);

            VkBuffer vertexBuf = vb.buffer();
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuf, &offset);

            vkCmdBindIndexBuffer(cmdBuf, ib.buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            ctx.registry.view<WorldTextComponent, Transform>().each([&](WorldTextComponent& wtc, Transform& tf) {
                FontShaderPushConstants pc;
                pc.model = tf.getMatrix();
                pc.textureIdx = getFont(wtc.font).index;
                vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                vkCmdDrawIndexed(cmdBuf, wtc.text.size() * 6, 1, wtc.idxOffset, 0, 0);
            });
        }
    }
}
