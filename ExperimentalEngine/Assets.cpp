#include "EditorWindows.hpp"
#include "imgui.h"
#include <filesystem>
#include "SourceModelLoader.hpp"
#include "CreateModelObject.hpp"

namespace worlds {
    void Assets::draw(entt::registry& reg) {
        struct EnumerateCallbackArgs {
            entt::registry& reg;
            std::string& currentDir;
        };

        static std::string currentDir = "";
        if (ImGui::Begin("Assets", &active)) {
            EnumerateCallbackArgs enumCallbackArgs{
                reg,
                currentDir
            };

            if (ImGui::Button("..")) {
                std::filesystem::path p{ currentDir };
                currentDir = p.parent_path().string();
                if (currentDir == "/")
                    currentDir = "";

                if (currentDir[0] == '/') {
                    currentDir = currentDir.substr(1);
                }
                logMsg("Navigated to %s", currentDir.c_str());
            }

            PHYSFS_enumerate(currentDir.c_str(), [](void* regPtr, const char* origDir, const char* fName) {
                EnumerateCallbackArgs* callbackArgs = (EnumerateCallbackArgs*)regPtr;
                entt::registry& reg = callbackArgs->reg;

                std::string origDirStr = origDir;
                if (origDirStr[0] == '/') {
                    origDirStr = origDirStr.substr(1);
                }

                std::string fullPath;

                if (origDirStr.empty())
                    fullPath = fName;
                else
                    fullPath = origDirStr + "/" + std::string(fName);

                PHYSFS_Stat stat;
                PHYSFS_stat(fullPath.c_str(), &stat);

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    if (ImGui::Button(fName)) {
                        if (callbackArgs->currentDir != "/")
                            callbackArgs->currentDir += "/";
                        callbackArgs->currentDir += fName;

                        if (currentDir[0] == '/') {
                            currentDir = currentDir.substr(1);
                        }
                        logMsg("Navigated to %s", currentDir.c_str());
                        return PHYSFS_ENUM_STOP;
                    }
                } else {
                    AssetID id = g_assetDB.addOrGetExisting(fullPath);
                    if (g_assetDB.getAssetExtension(id) == ".obj" || g_assetDB.getAssetExtension(id) == ".mdl") {
                        if (ImGui::Button(fName)) {
                            entt::entity ent = createModelObject(reg, glm::vec3(), glm::quat(), id, g_assetDB.addOrGetExisting("Materials/dev.json"));

                            if (g_assetDB.getAssetExtension(id) == ".mdl") {
                                WorldObject& wo = reg.get<WorldObject>(ent);
                                setupSourceMaterials(id, wo);
                            }
                        }
                    } else {
                        ImGui::Text("%s (%s)", fName, g_assetDB.getAssetExtension(id).c_str());
                    }
                }
                return PHYSFS_ENUM_OK;
                }, &enumCallbackArgs);
        }

        ImGui::End();
    }
}