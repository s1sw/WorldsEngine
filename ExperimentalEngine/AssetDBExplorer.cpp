#include "EditorWindows.hpp"
#include "AssetDB.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "Log.hpp"

namespace worlds {
    void AssetDBExplorer::draw(entt::registry& reg) {
        static AssetID currentRename = ~0u;
        static std::string renamePath = "";
        

        if (ImGui::Begin("AssetDB Explorer", &active)) {
            for (auto& p : g_assetDB.ids) {
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
                    g_assetDB.rename(currentRename, renamePath);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();

        
    }
}