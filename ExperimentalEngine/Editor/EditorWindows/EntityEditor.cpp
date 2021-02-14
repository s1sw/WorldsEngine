#include "EditorWindows.hpp"
#include "imgui.h"
#include "ComponentMetadata.hpp"
#include "Engine.hpp"
#include "imgui_stdlib.h"
#include "IconsFontAwesome5.h"

namespace worlds {
    void EntityEditor::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_CUBE u8" Selected entity", &active)) {
            entt::entity selectedEnt = editor->getSelectedEntity();

            if (reg.valid(selectedEnt)) {
                ImGui::Separator();

                for (auto& mdata : ComponentMetadataManager::sorted) {
                    ENTT_ID_TYPE t[] = { mdata->getComponentID() };
                    auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));

                    if (rtView.contains(selectedEnt)) {
                        mdata->edit(selectedEnt, reg);
                    }
                }

                bool justOpened = false;

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("Select Component");
                    justOpened = true;
                }

                ImGuiWindowClass cl;
                cl.ViewportFlagsOverrideSet = ImGuiViewportFlags_TopMost;
                cl.ParentViewportId = ImGui::GetWindowViewport()->ParentViewportId;
                ImGui::SetNextWindowClass(&cl);
                if (ImGui::BeginPopup("Select Component")) {
                    static std::string searchTxt;
                    static std::vector<ComponentEditor*> filteredMetadata;

                    if (justOpened) {
                        ImGui::SetKeyboardFocusHere();
                        searchTxt.clear();
                        justOpened = false;

                        filteredMetadata.clear();
                        filteredMetadata.reserve(ComponentMetadataManager::metadata.size());
                        for (auto& pair : ComponentMetadataManager::metadata) {
                            filteredMetadata.emplace_back(pair.second);
                        }
                    }

                    if (ImGui::InputText("Search", &searchTxt)) {
                        filteredMetadata.clear();
                        filteredMetadata.reserve(ComponentMetadataManager::metadata.size());
                        for (auto& pair : ComponentMetadataManager::metadata) {
                            filteredMetadata.emplace_back(pair.second);
                        }

                        if (!searchTxt.empty()) {
                            std::string lSearchTxt = searchTxt;

                            std::transform(lSearchTxt.begin(), lSearchTxt.end(), lSearchTxt.begin(),
                                [](unsigned char c) { return std::tolower(c); });

                            filteredMetadata.erase(std::remove_if(filteredMetadata.begin(), filteredMetadata.end(), [&lSearchTxt](ComponentEditor* mdata) {
                                std::string cName = mdata->getName();
                                std::transform(cName.begin(), cName.end(), cName.begin(),
                                    [](unsigned char c) { return std::tolower(c); });
                                size_t position = cName.find(lSearchTxt);
                                return position == std::string::npos;
                                }), filteredMetadata.end());
                        }
                    }

                    for (auto& mdata : filteredMetadata) {
                        ENTT_ID_TYPE t[] = { mdata->getComponentID() };
                        auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));
                        if (rtView.contains(selectedEnt))
                            continue;

                        if (ImGui::Button(mdata->getName())) {
                            mdata->create(selectedEnt, reg);
                        }
                    }
                    ImGui::EndPopup();
                }
            } else {
                ImGui::Text("Nothing selected");
            }
        }

        ImGui::End();
    }
}