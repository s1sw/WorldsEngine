#include "EditorWindows.hpp"
#include "Navigation/Navigation.hpp"
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <ImGui/imgui.h>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <Render/RenderInternal.hpp>
#include <Util/EnumUtil.hpp>
#include <glm/gtc/quaternion.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Audio/Audio.hpp>
#include <Core/MeshManager.hpp>
#include <Editor/GuiUtil.hpp>
#include <Recast.h>
#include <filesystem>
#include <stb_image_write.h>

namespace worlds
{
    void stbiWriteFunc(void* ctx, void* data, int bytes)
    {
        PHYSFS_writeBytes((PHYSFS_File*)ctx, data, bytes);
    }

    // void bakeCubemap(Editor* ed, glm::vec3 pos, worlds::VKRenderer* renderer,
    //    std::string name, entt::registry& world, int resolution, int iterations = 1) {
    //    // create RTT pass
    //    RTTPassSettings rtci;
    //    Camera cam;
    //    rtci.cam = &cam;
    //    rtci.enableShadows = true;
    //    rtci.width = resolution;
    //    rtci.height = resolution;
    //    rtci.isVr = false;
    //    rtci.outputToScreen = false;
    //    rtci.useForPicking = false;
    //    rtci.msaaLevel = 1;
    //    rtci.renderDebugShapes = false;
    //    rtci.staticsOnly = true;
    //    cam.verticalFOV = glm::radians(90.0f);
    //    cam.position = pos;

    //    VKRTTPass* rttPass = renderer->createRTTPass(rtci);

    //    const glm::vec3 directions[6] = {
    //        glm::vec3(1.0f, 0.0f, 0.0f),
    //        glm::vec3(-1.0f, 0.0f, 0.0f),
    //        glm::vec3(0.0f, -1.0f, 0.0f),
    //        glm::vec3(0.0f, 1.0f, 0.0f),
    //        glm::vec3(0.0f, 0.0f, -1.0f),
    //        glm::vec3(0.0f, 0.0f, 1.0f)
    //    };

    //    const glm::vec3 upDirs[6] = {
    //        glm::vec3(0.0f, 1.0f, 0.0f),
    //        glm::vec3(0.0f, 1.0f, 0.0f),
    //        glm::vec3(0.0f, 0.0f, -1.0f),
    //        glm::vec3(0.0f, 0.0f, 1.0f),
    //        glm::vec3(0.0f, 1.0f, 0.0f),
    //        glm::vec3(0.0f, 1.0f, 0.0f)
    //    };

    //    const std::string outputNames[6] = {
    //        "px",
    //        "nx",
    //        "py",
    //        "ny",
    //        "pz",
    //        "nz"
    //    };

    //    std::vector<std::string> outputPaths;
    //    logMsg("baking cubemap %s", name.c_str());

    //    std::string levelCubemapPath = "LevelData/Cubemaps/" + world.ctx<SceneInfo>().name + "/";
    //    auto jsonPath = levelCubemapPath + name + ".json";
    //    std::filesystem::create_directories(std::string(ed->currentProject().builtData()) + "/" + levelCubemapPath);
    //    AssetID cubemapId = AssetDB::pathToId(jsonPath);
    //    auto resources = renderer->getResources();

    //    uint32_t missingSlot = resources.cubemaps.get(AssetDB::pathToId("Cubemaps/missing.json"));
    //    uint32_t cubemapSlot = resources.cubemaps.get(cubemapId);
    //    bool isMissing = cubemapSlot != missingSlot;
    //    bool isCubemapLoaded = resources.cubemaps.isLoaded(cubemapId) && !isMissing;

    //    VulkanHandles* vkHandles = renderer->getHandles();
    //    VkQueue queue;
    //    vkGetDeviceQueue(vkHandles->device, vkHandles->graphicsQueueFamilyIdx, 0, &queue);

    //    if (isCubemapLoaded) {
    //        auto idx = resources.cubemaps.get(cubemapId);
    //        vku::TextureImageCube& loadedCube = resources.cubemaps[idx];

    //        vku::executeImmediately(vkHandles->device, vkHandles->commandPool, queue, [&](VkCommandBuffer cmdBuf) {
    //            loadedCube.setLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
    //                VK_PIPELINE_STAGE_TRANSFER_BIT);
    //            });
    //    }

    //    renderer->startRdocCapture();
    //    for (int iteration = 0; iteration < iterations; iteration++) {
    //        // for every face:
    //        // 1. setup camera
    //        // 2. run RTT pass
    //        // 3. get HDR data
    //        // 4. save to file
    //        for (int i = 0; i < 6; i++) {
    //            logMsg("baking face %i...", i);
    //            cam.rotation = glm::quatLookAt(directions[i], upDirs[i]);
    //            rttPass->drawNow(world);

    //            if (isCubemapLoaded) {
    //                // If the cubemap's loaded, we want to blit the image directly to the in-memory cubemap
    //                // so we don't have to load it off the disk again.
    //                auto idx = resources.cubemaps.get(cubemapId);
    //                vku::TextureImageCube& loadedCube = resources.cubemaps[idx];

    //                VkImageBlit ib;
    //                ib.dstSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)i, 1 };
    //                ib.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
    //                ib.dstOffsets[1] = VkOffset3D{ (int)loadedCube.extent().width, (int)loadedCube.extent().height, 1
    //                }; ib.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    //                ib.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
    //                ib.srcOffsets[1] = VkOffset3D{ (int)rttPass->width, (int)rttPass->height, 1 };

    //                vku::executeImmediately(vkHandles->device, vkHandles->commandPool, queue, [&](VkCommandBuffer cb)
    //                {
    //                    vku::GenericImage& srcImg = rttPass->hdrTarget->image();

    //                    loadedCube.setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //                        VK_PIPELINE_STAGE_TRANSFER_BIT,
    //                        VK_ACCESS_TRANSFER_WRITE_BIT);

    //                    srcImg.setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    //                        VK_PIPELINE_STAGE_TRANSFER_BIT,
    //                        VK_ACCESS_TRANSFER_READ_BIT);

    //                    vkCmdBlitImage(cb, srcImg.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, loadedCube.image(),
    //                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ib, VK_FILTER_LINEAR);

    //                    srcImg.setLayout(cb,
    //                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    //                    loadedCube.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //                        VK_ACCESS_SHADER_READ_BIT);
    //                    });
    //            }

    //            // Don't save unless this is the final iteration
    //            if (iteration == iterations - 1) {
    //                float* data = rttPass->getHDRData();
    //                auto outPath = levelCubemapPath + name + outputNames[i] + ".hdr";
    //                PHYSFS_File* fHandle = PHYSFS_openWrite(("Data/" + outPath).c_str());
    //                if (fHandle == nullptr) {
    //                    logErr("Failed to open cubemap file as write");
    //                    addNotification("Failed to bake cubemap (couldn't open file)", NotificationType::Error);
    //                    free(data);
    //                    renderer->destroyRTTPass(rttPass);
    //                    return;
    //                }

    //                int retVal = stbi_write_hdr_to_func(
    //                    stbiWriteFunc,
    //                    (void*)fHandle,
    //                    rtci.width, rtci.height, 4, data);

    //                if (retVal == 0) {
    //                    logErr(("Failed to write cubemap " + outPath).c_str());
    //                    addNotification("Failed to bake cubemap (couldn't encode)", NotificationType::Error);
    //                }

    //                PHYSFS_close(fHandle);

    //                outputPaths.push_back(outPath);
    //                free(data);
    //            }
    //        }

    //        if (isCubemapLoaded) {
    //            // Reconvolute - we only replaced the lowest mip
    //            auto idx = resources.cubemaps.get(cubemapId);
    //            vku::TextureImageCube& loadedCube = resources.cubemaps[idx];
    //            resources.cubemaps.convoluter()->convolute(loadedCube);
    //        }
    //    }

    //    renderer->endRdocCapture();
    //    auto file = PHYSFS_openWrite(("Data/" + jsonPath).c_str());

    //    std::string j = "[\n";
    //    for (int i = 0; i < 6; i++) {
    //        j += "    \"" + outputPaths[i] + "\"";
    //        if (i != 5) j += ",";
    //        j += "\n";
    //    }
    //    j += "]";

    //    PHYSFS_writeBytes(file, j.c_str(), j.size());
    //    PHYSFS_close(file);

    //    renderer->destroyRTTPass(rttPass);

    //    if (isMissing) {
    //        resources.cubemaps.unload(cubemapSlot);
    //    }
    //}

    void BakingWindow::draw(entt::registry& reg)
    {
        if (ImGui::Begin(ICON_FA_COOKIE u8" Baking", &active))
        {
            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio"))
            {
                uint32_t staticAudioGeomCount = 0;

                reg.view<WorldObject>().each([&](auto, WorldObject& wo) {
                    if (enumHasFlag(wo.staticFlags, StaticFlags::Audio))
                        staticAudioGeomCount++;
                });

                if (staticAudioGeomCount > 0)
                {
                    ImGui::Text("%u static geometry objects", staticAudioGeomCount);
                }
                else
                {
                    ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f),
                                       "There aren't any objects marked as audio static in the scene.");
                }

                // if (ImGui::Button("Bake")) {
                //    AudioSystem::getInstance()->bakeProbes(reg);
                //}
                if (ImGui::Button("Bake Geometry"))
                {
                    std::string savedPath = "Data/LevelData/PhononScenes/" + reg.ctx<SceneInfo>().name + ".bin";
                    AudioSystem::getInstance()->saveAudioScene(reg, savedPath.c_str());
                }
            }

            if (ImGui::CollapsingHeader(ICON_FA_CUBE u8" Cubemaps"))
            {
                static int numIterations = 1;
                ImGui::DragInt("Iterations", &numIterations);
                reg.view<Transform, WorldCubemap, NameComponent>().each(
                    [&](Transform& t, WorldCubemap& wc, NameComponent& nc) {
                        ImGui::Text("%s (%.2f, %.2f, %.2f)", nc.name.c_str(), t.position.x, t.position.y, t.position.z);
                        ImGui::SameLine();
                        ImGui::PushID(nc.name.c_str());
                        if (ImGui::Button("Bake"))
                        {
                            // bakeCubemap(editor, t.position + wc.captureOffset,
                            // static_cast<worlds::VKRenderer*>(interfaces.renderer), nc.name, reg, wc.resolution,
                            // numIterations);
                        }
                        ImGui::PopID();
                    });
            }

            if (ImGui::CollapsingHeader(ICON_FA_MAP u8" Navigation"))
            {
                glm::vec3 bbMin{FLT_MAX};
                glm::vec3 bbMax{-FLT_MAX};
                reg.view<Transform, WorldObject>().each([&](Transform& t, WorldObject& o) {
                    if (!enumHasFlag(o.staticFlags, StaticFlags::Navigation))
                        return;

                    glm::mat4 tMat = t.getMatrix();
                    auto& mesh = MeshManager::loadOrGet(o.mesh);
                    for (const Vertex& v : mesh.vertices)
                    {
                        glm::vec3 transformedPosition = tMat * glm::vec4(v.position, 1.0f);
                        bbMin = glm::min(transformedPosition, bbMin);
                        bbMax = glm::max(transformedPosition, bbMax);
                    }
                });
                ImGui::Text("World Bounds: (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f)", bbMin.x, bbMin.y, bbMin.z,
                            bbMax.x, bbMax.y, bbMax.z);

                if (ImGui::Button("Bake"))
                {
                    std::string savedPath = "Data/LevelData/Navmeshes/" + reg.ctx<SceneInfo>().name + ".bin";
                    NavigationSystem::buildAndSave(reg, savedPath.c_str());
                }
            }
        }
        ImGui::End();
    }
}
