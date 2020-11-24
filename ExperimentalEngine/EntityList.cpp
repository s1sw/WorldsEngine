#include "EditorWindows.hpp"
#include "imgui.h"
#include "NameComponent.hpp"
#include "IconsFontAwesome5.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include "Log.hpp"

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

                if (currentlyRenaming != ent) {
                    if (nc == nullptr) {
                        ImGui::Text("Entity %u", ent);
                    } else {
                        ImGui::TextUnformatted(nc->name.c_str());
                    }
                } else {
                    if (ImGui::InputText("###name", &nc->name, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        currentlyRenaming = entt::null;
                    }
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    logMsg("dbl click: %s", nc->name.c_str());
                    currentlyRenaming = ent;
                }

                ImGui::SameLine();
                if (ImGui::Button("Select"))
                    editor->select(ent);
                ImGui::PopID();
                };

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
        ImGui::End();
    }
}
