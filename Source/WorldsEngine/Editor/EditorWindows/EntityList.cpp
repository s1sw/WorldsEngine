#include "EditorWindows.hpp"
#include <Core/HierarchyUtil.hpp>
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <Core/WorldComponents.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <Libs/IconsFontAwesome5.h>
#include <algorithm>

namespace worlds
{
    void EntityList::draw(entt::registry& reg)
    {
        static std::string searchText;
        static std::vector<entt::entity> filteredEntities;
        static size_t numNamedEntities;
        static bool showUnnamed = false;
        static entt::entity currentlyRenaming = entt::null;
        static entt::entity popupOpenFor = entt::null;

        if (ImGui::Begin(ICON_FA_LIST u8" Entity List", &active))
        {
            size_t currNamedEntCount = reg.view<NameComponent>().size();
            bool searchNeedsUpdate = !searchText.empty() && numNamedEntities != currNamedEntCount;

            if (ImGui::InputText("Search", &searchText) || searchNeedsUpdate)
            {
                std::string lSearchTxt = searchText;
                std::transform(lSearchTxt.begin(), lSearchTxt.end(), lSearchTxt.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                filteredEntities.clear();
                reg.view<NameComponent>().each([&](auto ent, NameComponent& nc)
                {
                    std::string name = nc.name;

                    std::transform(name.begin(), name.end(), name.begin(),
                                   [](unsigned char c) { return std::tolower(c); });

                    size_t pos = name.find(lSearchTxt);

                    if (pos != std::string::npos)
                    {
                        filteredEntities.push_back(ent);
                    }
                });
            }

            numNamedEntities = currNamedEntCount;
            ImGui::Checkbox("Show Unnamed Entities", &showUnnamed);

            if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseClicked[1])
            {
                ImGui::OpenPopup("AddEntity");
            }


            bool openEntityContextMenu = false;
            bool navDown = false;
            bool navUp = false;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsWindowFocused(
                ImGuiFocusedFlags_RootAndChildWindows))
            {
                navDown = ImGui::IsKeyPressed(SDL_SCANCODE_DOWN);
                navUp = ImGui::IsKeyPressed(SDL_SCANCODE_UP);
            }
            else
            {
                navDown = false;
                navUp = false;
            }

            bool selectNext = false;
            entt::entity lastEntity = entt::null;
            std::function<void(entt::entity)> forEachEnt = [&](entt::entity ent)
            {
                ImGui::PushID((int)ent);
                auto nc = reg.try_get<NameComponent>(ent);
                float lineHeight = ImGui::CalcTextSize("w").y;

                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float windowWidth = ImGui::GetWindowWidth();
                ImVec2 windowPos = ImGui::GetWindowPos();

                if (selectNext)
                {
                    editor->select(ent);
                    selectNext = false;
                }

                if (editor->isEntitySelected(ent))
                {
                    drawList->AddRectFilled(
                        ImVec2(0.0f + windowPos.x, cursorPos.y + windowPos.y - ImGui::GetScrollY()),
                        ImVec2(windowWidth + windowPos.x, cursorPos.y + lineHeight + windowPos.y - ImGui::GetScrollY()),
                        ImColor(0, 75, 150));

                    if (navDown)
                    {
                        selectNext = true;
                        navDown = false;
                    }

                    if (navUp)
                    {
                        if (lastEntity != entt::null)
                            editor->select(lastEntity);
                        navUp = false;
                    }
                }

                lastEntity = ent;

                if (currentlyRenaming != ent)
                {
                    if (nc == nullptr)
                    {
                        ImGui::Text("Entity %u", static_cast<uint32_t>(ent));
                    }
                    else
                    {
                        ImGui::TextUnformatted(nc->name.c_str());
                    }
                }
                else
                {
                    if (nc == nullptr)
                    {
                        currentlyRenaming = entt::null;
                    }
                    else if (ImGui::InputText("###name", &nc->name, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        currentlyRenaming = entt::null;
                    }
                }

                // Parent drag/drop
                ImGuiDragDropFlags entityDropFlags = 0 | ImGuiDragDropFlags_SourceNoDisableHover |
                    ImGuiDragDropFlags_SourceNoHoldToOpenOthers |
                    ImGuiDragDropFlags_SourceAllowNullID;

                if (ImGui::BeginDragDropSource(entityDropFlags))
                {
                    if (nc == nullptr)
                    {
                        ImGui::Text("Entity %u", static_cast<uint32_t>(ent));
                    }
                    else
                    {
                        ImGui::TextUnformatted(nc->name.c_str());
                    }

                    ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &ent, sizeof(entt::entity));
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
                    {
                        assert(payload->DataSize == sizeof(entt::entity));
                        entt::entity droppedEntity = *reinterpret_cast<entt::entity*>(payload->Data);

                        if (!HierarchyUtil::isEntityChildOf(reg, droppedEntity, ent))
                            HierarchyUtil::setEntityParent(reg, droppedEntity, ent);
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    currentlyRenaming = ent;

                    if (nc == nullptr)
                    {
                        nc = &reg.emplace<NameComponent>(ent);
                        nc->name = "Entity";
                    }
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    popupOpenFor = ent;
                    openEntityContextMenu = true;
                    ImGui::OpenPopup("Entity Context Menu");
                }

                if (ImGui::IsItemClicked())
                {
                    if (!interfaces.inputManager->keyHeld(SDL_SCANCODE_LSHIFT))
                    {
                        editor->select(ent);
                    }
                    else
                    {
                        editor->multiSelect(ent);
                    }
                }

                if (reg.has<ParentComponent>(ent))
                {
                    auto& pc = reg.get<ParentComponent>(ent);

                    entt::entity currentChild = pc.firstChild;

                    ImGui::Indent();

                    while (reg.valid(currentChild))
                    {
                        auto& childComponent = reg.get<ChildComponent>(currentChild);

                        forEachEnt(currentChild);

                        currentChild = childComponent.nextChild;
                    }

                    ImGui::Unindent();
                }
                ImGui::PopID();
            };

            static uint32_t renamingFolder = 0u;

            if (ImGui::BeginChild("Entities"))
            {
                if (ImGui::BeginPopupContextWindow("AddEntity"))
                {
                    if (ImGui::Button("Empty"))
                    {
                        auto emptyEnt = reg.create();
                        Transform& emptyT = reg.emplace<Transform>(emptyEnt);
                        reg.emplace<NameComponent>(emptyEnt).name = "Empty";
                        editor->select(emptyEnt);
                        Camera& cam = editor->getFirstSceneView()->getCamera();
                        emptyT.position = cam.position + cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::Button("Point Light"))
                    {
                        auto emptyEnt = reg.create();
                        Transform& emptyT = reg.emplace<Transform>(emptyEnt);
                        reg.emplace<NameComponent>(emptyEnt).name = "Point Light";
                        editor->select(emptyEnt);
                        Camera& cam = editor->getFirstSceneView()->getCamera();
                        emptyT.position = cam.position + cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                        auto& light = reg.emplace<WorldLight>(emptyEnt);
                        light.type = LightType::Point;
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::Button("Spot Light"))
                    {
                        auto emptyEnt = reg.create();
                        Transform& emptyT = reg.emplace<Transform>(emptyEnt);
                        reg.emplace<NameComponent>(emptyEnt).name = "Spot Light";
                        editor->select(emptyEnt);
                        Camera& cam = editor->getFirstSceneView()->getCamera();
                        emptyT.position = cam.position + cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                        emptyT.rotation = cam.rotation;
                        auto& light = reg.emplace<WorldLight>(emptyEnt);
                        light.type = LightType::Spot;
                        light.spotOuterCutoff = glm::radians(50.0f);
                        light.spotCutoff = glm::radians(20.0f);
                        light.maxDistance = 5.0f;
                        light.intensity = 3.0f;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (searchText.empty())
                {
                    if (showUnnamed)
                    {
                        reg.view<Transform>(entt::exclude_t<ChildComponent>{}).each(
                            [&](entt::entity ent, const Transform&) { forEachEnt(ent); });
                    }
                    else
                    {
                        reg.view<NameComponent>(entt::exclude_t<ChildComponent>{})
                           .each([&](auto ent, NameComponent) { forEachEnt(ent); });
                    }
                }
                else
                {
                    for (auto& ent : filteredEntities)
                        forEachEnt(ent);
                }
            }
            ImGui::EndChild();

            bool openFolderPopup = false;

            // Using a lambda here for early exit
            auto entityPopup = [&](entt::entity e)
            {
                if (!reg.valid(e))
                {
                    ImGui::CloseCurrentPopup();
                    return;
                }

                if (ImGui::Button("Delete"))
                {
                    reg.destroy(e);
                    ImGui::CloseCurrentPopup();
                    return;
                }

                if (ImGui::Button("Rename"))
                {
                    currentlyRenaming = e;
                    ImGui::CloseCurrentPopup();
                }
            };

            if (openEntityContextMenu)
            {
                ImGui::OpenPopup("Entity Context Menu");
            }

            if (ImGui::BeginPopup("Entity Context Menu"))
            {
                entityPopup(popupOpenFor);
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
