#include "../../Core/Engine.hpp"
#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#include "../../Libs/IconsFontAwesome5.h"
#include "../../Libs/IconsFontaudio.h"
#include "../../Util/EnumUtil.hpp"
#include "../../Render/Render.hpp"
#include "../../Core/NameComponent.hpp"
#include "../../Core/Log.hpp"
#include "glm/gtc/quaternion.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace worlds {
    void stbiWriteFunc(void* ctx, void* data, int bytes) {
        PHYSFS_writeBytes((PHYSFS_File*)ctx, data, bytes);
    }

    class CubemapBakeRoutine {
    public:
        CubemapBakeRoutine(glm::vec3 pos, worlds::VKRenderer* renderer, std::string name, entt::registry& world) {
        }
    private:
    };

    void bakeCubemap(glm::vec3 pos, worlds::VKRenderer* renderer,
            std::string name, entt::registry& world) {
        // create RTT pass
        RTTPassCreateInfo rtci;
        Camera cam;
        rtci.cam = &cam;
        rtci.enableShadows = true;
        rtci.width = 512;
        rtci.height = 512;
        rtci.isVr = false;
        rtci.outputToScreen = false;
        rtci.useForPicking = false;
        cam.verticalFOV = glm::radians(90.0f);
        cam.position = pos;

        RTTPassHandle rttHandle = renderer->createRTTPass(rtci);

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

        const std::string outputNames[6] = {
            "px",
            "nx",
            "py",
            "ny",
            "pz",
            "nz"
        };

        std::vector<std::string> outputPaths;
        logMsg("baking cubemap %s", name.c_str());

        // for each cubemap face:
        // 1. setup camera
        // 2. run RTT pass
        // 3. get HDR data
        // 4. save to file
        for (int i = 0; i < 6; i++) {
            logMsg("baking face %i...", i);
            cam.rotation = glm::quatLookAt(directions[i], upDirs[i]);
            renderer->updatePass(rttHandle, world);

            float* data = renderer->getPassHDRData(rttHandle);
            auto outPath = "LevelCubemaps/" + name + outputNames[i] + ".hdr";
            PHYSFS_File* fHandle = PHYSFS_openWrite(outPath.c_str());
            if (fHandle == nullptr) {
                logErr("Failed to open cubemap file as write");
                free(data);
                renderer->destroyRTTPass(rttHandle);
                return;
            }

            int retVal = stbi_write_hdr_to_func(
                    stbiWriteFunc,
                    (void*)fHandle,
                    512, 512, 4, data);

            if (retVal == 0) {
                logErr(("Failed to write cubemap " + outPath).c_str());
            }
            PHYSFS_close(fHandle);

            outputPaths.push_back(outPath);
            free(data);
        }

        auto jsonPath = "LevelCubemaps/" + name + ".json";
        auto file = PHYSFS_openWrite(jsonPath.c_str());

        std::string j = "[\n";
        for (int i = 0; i < 6; i++) {
            j += "    \"" + outputPaths[i] + "\"";
            if (i != 5) j += ",";
            j += "\n";
        }
        j += "]";

        PHYSFS_writeBytes(file, j.c_str(), j.size());
        PHYSFS_close(file);

        renderer->destroyRTTPass(rttHandle);
        auto resources = renderer->getResources();
        auto idx = resources.cubemaps.get(g_assetDB.addOrGetExisting(jsonPath));
        renderer->getResources().cubemaps.unload(idx);
    }

    void BakingWindow::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_COOKIE u8" Baking", &active)) {
            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio")) {
                uint32_t staticAudioGeomCount = 0;

                reg.view<WorldObject>().each([&](auto, WorldObject& wo) {
                    if (enumHasFlag(wo.staticFlags, StaticFlags::Audio))
                        staticAudioGeomCount++;
                });

                if (staticAudioGeomCount > 0) {
                    ImGui::Text("%u static geometry objects", staticAudioGeomCount);
                } else {
                    ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f),
                        "There aren't any objects marked as audio static in the scene.");
                }
            }

            if (ImGui::CollapsingHeader(ICON_FA_CUBE u8" Cubemaps")) {
                static int numIterations = 1;
                ImGui::DragInt("Iterations", &numIterations);
                reg.view<Transform, WorldCubemap, NameComponent>().each([&](auto,
                            Transform& t, WorldCubemap&, NameComponent& nc) {
                    ImGui::Text("%s (%.2f, %.2f, %.2f)", nc.name.c_str(),
                            t.position.x, t.position.y, t.position.z);
                    ImGui::SameLine();
                    ImGui::PushID(nc.name.c_str());
                    if (ImGui::Button("Bake")) {
                        for (int i = 0; i < numIterations; i++)
                            bakeCubemap(t.position, interfaces.renderer, nc.name, reg);
                    }
                    ImGui::PopID();
                });
            }
        }
        ImGui::End();
    }
}
