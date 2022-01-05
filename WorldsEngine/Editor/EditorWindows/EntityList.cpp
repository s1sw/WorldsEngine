#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#include "../../Core/NameComponent.hpp"
#include "../../Libs/IconsFontAwesome5.h"
#include "../../ImGui/imgui_stdlib.h"
#include <algorithm>
#include "../../Core/Log.hpp"

namespace worlds {
    void showFolderButtons(entt::entity e, EntityFolder& folder, int counter) {
        if (ImGui::Button((folder.name + "##" + std::to_string(counter)).c_str())) {
            folder.entities.push_back(e);
            ImGui::CloseCurrentPopup();
        }

        for (EntityFolder& c : folder.children) {
            showFolderButtons(e, c, counter++);
        }
    }

    void EntityList::draw(entt::registry& reg) {
        static std::string searchText;
        static std::vector<entt::entity> filteredEntities;
        static size_t numNamedEntities;
        static bool showUnnamed = false;
        static bool folderView = true;
        static entt::entity currentlyRenaming = entt::null;
        static entt::entity popupOpenFor = entt::null;

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
            ImGui::Checkbox("Folder View", &folderView);

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

            bool openEntityContextMenu = false;

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

                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    popupOpenFor = ent;
                    openEntityContextMenu = true;
                    ImGui::OpenPopup("Entity Context Menu");
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


            EntityFolder* renamingFolder = nullptr;

            std::function<void(EntityFolder&, int)> doFolderEntry = [&](EntityFolder& folder, int folderNum) {
                bool thisFolderRenaming = &folder == renamingFolder;

                std::string label;

                if (thisFolderRenaming)
                    label = "##" + std::to_string(folderNum);
                else
                    label = folder.name + "##" + std::to_string(folderNum);

                ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_None;

                if (thisFolderRenaming) {
                    treeNodeFlags |= ImGuiTreeNodeFlags_AllowItemOverlap;
                }

                if (ImGui::TreeNodeEx(label.c_str(), treeNodeFlags)) {
                    if (renamingFolder) {
                        if (ImGui::InputText("##foldername", &folder.name, ImGuiInputTextFlags_EnterReturnsTrue)) {
                            renamingFolder = nullptr;
                        }
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        renamingFolder = &folder;
                    }

                    for (entt::entity ent : folder.entities) {
                        forEachEnt(ent);
                    }

                    for (EntityFolder& child : folder.children) {
                        doFolderEntry(child, folderNum++);
                    }

                    if (ImGui::Button("+")) {
                        folder.children.push_back(EntityFolder{.name = "Untitled Entity Folder"});
                    }
                    ImGui::TreePop();
                }
            };

            if (ImGui::BeginChild("Entities")) {
                if (searchText.empty()) {
                    if (folderView) {
                        EntityFolders& folders = reg.ctx<EntityFolders>();
                        doFolderEntry(folders.rootFolder, 0);
                    } else {
                        if (showUnnamed) {
                            reg.each(forEachEnt);
                        } else {
                            reg.view<NameComponent>().each([&](auto ent, NameComponent) {
                                forEachEnt(ent);
                            });
                        }
                    }
                } else {
                    for (auto& ent : filteredEntities)
                        forEachEnt(ent);
                }

            }
            ImGui::EndChild();

            bool openFolderPopup = false;

            // Using a lambda here for early exit
            auto entityPopup = [&](entt::entity e) {
                if (!reg.valid(e)) {
                    ImGui::CloseCurrentPopup();
                    return;
                }

                if (ImGui::Button("Delete")) {
                    reg.destroy(e);
                    ImGui::CloseCurrentPopup();
                    return;
                }

                if (ImGui::Button("Rename")) {
                    currentlyRenaming = e;
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::Button("Add to folder")) {
                    ImGui::CloseCurrentPopup();
                    openFolderPopup = true;
                }
            };

            if (openEntityContextMenu) {
                ImGui::OpenPopup("Entity Context Menu");
            }

            if (ImGui::BeginPopup("Entity Context Menu")) {
                entityPopup(popupOpenFor);
                ImGui::EndPopup();
            }

            auto folderPopup = [&](entt::entity e) {
                EntityFolders& folders = reg.ctx<EntityFolders>();
                showFolderButtons(e, folders.rootFolder, 0);
            };

            if (openFolderPopup)
                ImGui::OpenPopup("Add to folder");

            if (ImGui::BeginPopup("Add to folder")) {
                folderPopup(popupOpenFor);
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
