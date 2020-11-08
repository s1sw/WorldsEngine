#include "EditorWindows.hpp"
#include "imgui.h"
#include "ComponentMetadata.hpp"
#include "Engine.hpp"
#include "imgui_stdlib.h"
#include "IconsFontAwesome5.h"

namespace worlds {
    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" }
    };

    void EntityEditor::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_CUBE u8" Selected entity")) {
            entt::entity selectedEnt = editor->getSelectedEntity();

            if (reg.valid(selectedEnt)) {
                ImGui::Separator();

                for (auto& mdataPair : ComponentMetadataManager::metadata) {
                    auto& mdata = mdataPair.second;
                    ENTT_ID_TYPE t[] = { mdata.typeId };
                    auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));

                    if (rtView.contains(selectedEnt) && mdata.editFuncPtr != nullptr) {
                        mdata.editFuncPtr(selectedEnt, reg);
                    }
                }

                if (reg.has<WorldLight>(selectedEnt)) {
                    ImGui::Text("WorldLight");
                    ImGui::SameLine();
                    if (ImGui::Button("Remove##WL")) {
                        reg.remove<WorldLight>(selectedEnt);
                    } else {
                        auto& worldLight = reg.get<WorldLight>(selectedEnt);
                        ImGui::ColorEdit3("Color", &worldLight.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

                        if (ImGui::BeginCombo("Light Type", lightTypeNames.at(worldLight.type))) {
                            for (auto& p : lightTypeNames) {
                                bool isSelected = worldLight.type == p.first;
                                if (ImGui::Selectable(p.second, &isSelected)) {
                                    worldLight.type = p.first;
                                }

                                if (isSelected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        if (worldLight.type == LightType::Spot) {
                            ImGui::DragFloat("Spot Cutoff", &worldLight.spotCutoff);
                        }
                    }
                }

                bool justOpened = false;

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("Select Component");
                    justOpened = true;
                }

                if (ImGui::BeginPopup("Select Component")) {
                    static std::string searchTxt;
                    static std::vector<ComponentMetadata> filteredMetadata;

                    if (justOpened) {
                        ImGui::SetKeyboardFocusHere(0);
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

                            filteredMetadata.erase(std::remove_if(filteredMetadata.begin(), filteredMetadata.end(), [&lSearchTxt](ComponentMetadata& mdata) {
                                std::string cName = mdata.name;
                                std::transform(cName.begin(), cName.end(), cName.begin(),
                                    [](unsigned char c) { return std::tolower(c); });
                                size_t position = cName.find(lSearchTxt);
                                return position == std::string::npos;
                                }), filteredMetadata.end());
                        }
                    }

                    for (auto& mdata : filteredMetadata) {
                        ENTT_ID_TYPE t[] = { mdata.typeId };
                        auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));
                        if (rtView.contains(selectedEnt))
                            continue;

                        if (ImGui::Button(mdata.name.c_str())) {
                            mdata.addFuncPtr(selectedEnt, reg);
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