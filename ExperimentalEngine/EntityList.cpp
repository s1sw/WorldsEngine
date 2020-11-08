#include "EditorWindows.hpp"
#include "imgui.h"
#include "NameComponent.hpp"
#include "IconsFontAwesome5.h"

namespace worlds {
    void EntityList::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_LIST u8" Entity List", &active)) {
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

            reg.each([&](auto ent) {
                ImGui::PushID((int)ent);
                auto nc = reg.try_get<NameComponent>(ent);

                if (nc == nullptr) {
                    ImGui::Text("Entity %u", ent);
                } else {
                    ImGui::Text("%s", nc->name.c_str());
                }
                ImGui::SameLine();
                if (ImGui::Button("Select"))
                    editor->select(ent);
                ImGui::PopID();
                });
        }
        ImGui::End();
    }
}