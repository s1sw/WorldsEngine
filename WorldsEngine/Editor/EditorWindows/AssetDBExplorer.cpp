#include "EditorWindows.hpp"
#include "../../Core/AssetDB.hpp"
#include "../../ImGui/imgui.h"
#include "../../ImGui/imgui_stdlib.h"
#include "../../Core/Log.hpp"
#include <robin_hood.h>

namespace worlds {
    class AssetDB::ADBStorage {
    public:
        robin_hood::unordered_map<AssetID, std::string> paths;
        robin_hood::unordered_map<std::string, AssetID> ids;
        robin_hood::unordered_map<AssetID, std::string> extensions;
    };

    void AssetDBExplorer::draw(entt::registry& reg) {
        static AssetID currentRename = ~0u;
        static std::string renamePath = "";

        if (ImGui::Begin("AssetDB Explorer", &active)) {
            for (auto& p : g_assetDB.storage->ids) {
                ImGui::Text("%u: %s", p.second, p.first.c_str());

                if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
                    currentRename = p.second;
                    renamePath = p.first;
                    ImGui::OpenPopup("Rename Asset");
                }
            }

            if (ImGui::BeginPopup("Rename Asset")) {
                ImGui::Text("Renaming %u", currentRename);
                ImGui::InputText("", &renamePath);

                if (ImGui::Button("Rename")) {
                    ImGui::CloseCurrentPopup();
                    std::string oldPath = g_assetDB.getAssetPath(currentRename);
                    const char* realRootDir = PHYSFS_getRealDir(oldPath.c_str());
                    g_assetDB.rename(currentRename, renamePath);

                    // try to rename the physical file - this isn't possible if it's in an archive,
                    // but if that's the case we shouldn't be hitting this path anyway
                    if (realRootDir) {
                        // construct real path
                        std::string realPath = std::string{realRootDir} + '/' + oldPath;
                        std::string newPath = std::string{realRootDir} + '/' + renamePath;
                        logMsg("renaming %s to %s", realPath.c_str(), newPath.c_str());

                        rename(realPath.c_str(), newPath.c_str());
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();


    }
}
