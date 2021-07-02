#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#include "../../Core/NameComponent.hpp"
#include "../../Libs/IconsFontAwesome5.h"
#include "../../ImGui/imgui_stdlib.h"
#include <algorithm>
#include "../../Core/Log.hpp"

namespace worlds {
    void EntityList::draw(entt::registry& reg) {
        static std::string searchText;
        static std::vector<entt::entity> filteredEntities;
        static size_t numNamedEntities;
        static bool showUnnamed = false;
        static entt::entity currentlyRenaming = entt::null;

        if (ImGui::Begin(ICON_FA_LIST u8" Entity List", &active)) {
            size_t currNamedEntCount = reg.view<NameComponent>().size();
            bool searchNeedsUpdate = !searchText.empty() &&
                numNamedEntities != currNamedEntCount;

            if (ImGui::InputText("Search", &searchText) || searchNeedsUpdate) {
                std::string lSearchTxt = searchText;
                std::transform(
                    lSearchTxt.begin(), lSearchTxt.end(),
                    lSearchTxt.begin(),
                    [](unsigned char c) { return std::tolower(c); }
                );

                filteredEntities.clear();
                reg.view<NameComponent>().each([&](auto ent, NameComponent& nc) {
                    std::string name = nc.name;

                    std::transform(
                        name.begin(), name.end(),
                        name.begin(),
                        [](unsigned char c) { return std::tolower(c); }
                    );

                    size_t pos = name.find(lSearchTxt);

                    if (pos != std::string::npos) {
                        filteredEntities.push_back(ent);
                    }
                });
            }

            numNamedEntities = currNamedEntCount;
            ImGui::Checkbox("Show Unnamed Entities", &showUnnamed);

            if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseClicked[1]) {
                ImGui::OpenPopup("AddEntity");
            }

            if (ImGui::BeginPopupContextWindow("AddEntity")) {
                if (ImGui::Button("Empty")) {
                    auto emptyEnt = reg.create();
                    reg.emplace<Transform>(emptyEnt);
                    reg.emplace<NameComponent>(emptyEnt).name = "Empty";
                    editor->select(emptyEnt);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            auto forEachEnt = [&](entt::entity ent) {
                ImGui::PushID((int)ent);
                auto nc = reg.try_get<NameComponent>(ent);
                float lineHeight = ImGui::CalcTextSize("w").y;

                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float windowWidth = ImGui::GetWindowWidth();
                ImVec2 windowPos = ImGui::GetWindowPos();

                if (editor->isEntitySelected(ent)) {
                    drawList->AddRectFilled(
                        ImVec2(0.0f + windowPos.x, cursorPos.y + windowPos.y - ImGui::GetScrollY()),
                        ImVec2(windowWidth + windowPos.x, cursorPos.y + lineHeight + windowPos.y - ImGui::GetScrollY()),
                        ImColor(0, 75, 150)
                    );
                }

                if (currentlyRenaming != ent) {
                    if (nc == nullptr) {
                        ImGui::Text("Entity %u", static_cast<uint32_t>(ent));
                    } else {
                        ImGui::TextUnformatted(nc->name.c_str());
                    }
                } else {
                    if (nc == nullptr) {
                        currentlyRenaming = entt::null;
                    } else if (ImGui::InputText("###name", &nc->name, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        currentlyRenaming = entt::null;
                    }
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    currentlyRenaming = ent;

                    if (nc == nullptr) {
                        nc = &reg.emplace<NameComponent>(ent);
                        nc->name = "Entity";
                    }
                }

                if (ImGui::IsItemClicked()) {
                    if (!interfaces.inputManager->keyHeld(SDL_SCANCODE_LSHIFT)) {
                        editor->select(ent);
                    } else {
                        editor->multiSelect(ent);
                    }
                }
                ImGui::PopID();
                };

            if (ImGui::BeginChild("Entities")) {
                if (searchText.empty()) {
                    if (showUnnamed) {
                        reg.each(forEachEnt);
                    } else {
                        reg.view<NameComponent>().each([&](auto ent, NameComponent) {
                            forEachEnt(ent);
                        });
                    }
                } else {
                    for (auto& ent : filteredEntities)
                        forEachEnt(ent);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}
