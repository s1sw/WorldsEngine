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
#include <deque>

namespace worlds
{
    void stbiWriteFunc(void* ctx, void* data, int bytes)
    {
        PHYSFS_writeBytes((PHYSFS_File*)ctx, data, bytes);
    }

    class CubemapBaker
    {
        struct CubemapBakeOp
        {
            std::string name;
            glm::vec3 pos;
            int faceIdx = 0;
            bool waitingForResult = false;

            std::vector<std::string> outputPaths;
        };

        std::deque<CubemapBakeOp> cubemapBakeOps;

        const std::string outputNames[6] = {
            "px",
            "nx",
            "py",
            "ny",
            "pz",
            "nz"
        };

        const glm::vec3 directions[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        };

        const glm::vec3 upDirs[6] = {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        };
    public:
        RTTPass* rttPass;
        Camera cam;
        Renderer* renderer;
        entt::registry& world;

        CubemapBaker(Renderer* renderer, entt::registry& world)
            : renderer(renderer)
            , world(world)
        {
            // create RTT pass
            RTTPassSettings rtci;
            rtci.cam = &cam;
            rtci.enableShadows = true;
            rtci.width = 128;
            rtci.height = 128;
            rtci.useForPicking = false;
            rtci.msaaLevel = 1;
            rtci.renderDebugShapes = false;
            rtci.staticsOnly = true;
            cam.verticalFOV = glm::radians(90.0f);

            rttPass = renderer->createRTTPass(rtci);
        }

        void addToQueue(std::string name, glm::vec3 pos)
        {
            cubemapBakeOps.emplace_back(name, pos);
        }

        void update()
        {
            if (cubemapBakeOps.empty())
            {
                rttPass->active = false;
                return;
            }

            rttPass->active = true;

            CubemapBakeOp& currentOp = cubemapBakeOps.front();

            if (currentOp.waitingForResult)
            {
                float* data;
                if (rttPass->getHDRData(data))
                {
                    std::string levelCubemapPath = "LevelData/Cubemaps/" + world.ctx<SceneInfo>().name + "/";
                    auto outPath = levelCubemapPath + currentOp.name + outputNames[currentOp.faceIdx] + ".hdr";
                    PHYSFS_File* fHandle = PHYSFS_openWrite(("Data/" + outPath).c_str());
                    if (fHandle == nullptr) {
                        logErr("Failed to open cubemap file as write");
                        addNotification("Failed to bake cubemap (couldn't open file)", NotificationType::Error);
                        free(data);
                        return;
                    }

                    int retVal = stbi_write_hdr_to_func(
                        stbiWriteFunc,
                        (void*)fHandle,
                        rttPass->width, rttPass->height, 4, data);

                    if (retVal == 0) {
                        logErr(("Failed to write cubemap " + outPath).c_str());
                        addNotification("Failed to bake cubemap (couldn't encode)", NotificationType::Error);
                    }

                    PHYSFS_close(fHandle);

                    currentOp.outputPaths.push_back(outPath);
                    free(data);
                    currentOp.waitingForResult = false;

                    if (currentOp.faceIdx == 5)
                    {
                        // we're done baking!!
                        auto jsonPath = levelCubemapPath + currentOp.name + ".json";
                        auto file = PHYSFS_openWrite(("Data/" + jsonPath).c_str());

                        std::string j = "[\n";
                        for (int i = 0; i < 6; i++) {
                            j += "    \"" + currentOp.outputPaths[i] + "\"";
                            if (i != 5) j += ",";
                            j += "\n";
                        }
                        j += "]";

                        PHYSFS_writeBytes(file, j.c_str(), j.size());
                        PHYSFS_close(file);

                        AssetDB::notifyAssetChange(AssetDB::pathToId(jsonPath));
                        
                        cubemapBakeOps.pop_front();
                    }
                    else
                    {
                        currentOp.faceIdx++;
                    }
                }
            }
            else
            {
               cam.position = currentOp.pos;
               cam.rotation = glm::quatLookAt(directions[currentOp.faceIdx], upDirs[currentOp.faceIdx]);
               rttPass->requestHDRData();
               currentOp.waitingForResult = true;
            }
        }

        ~CubemapBaker()
        {
            renderer->destroyRTTPass(rttPass);
        }
    };

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
                static CubemapBaker* cb = nullptr;

                if (cb == nullptr)
                {
                    cb = new CubemapBaker(interfaces.renderer, reg);
                }
                cb->update();

                reg.view<Transform, WorldCubemap, NameComponent>().each(
                    [&](Transform& t, WorldCubemap& wc, NameComponent& nc) {
                        ImGui::Text("%s (%.2f, %.2f, %.2f)", nc.name.c_str(), t.position.x, t.position.y, t.position.z);
                        ImGui::SameLine();
                        ImGui::PushID(nc.name.c_str());
                        if (ImGui::Button("Bake"))
                        {
                            cb->addToQueue(nc.name, t.position + wc.captureOffset);
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
                NavigationSystem::drawNavMesh();

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
