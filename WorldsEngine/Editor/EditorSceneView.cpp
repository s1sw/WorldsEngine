#include "Editor.hpp"
#include "ImGui/ImGuizmo.h"
#include "Render/Render.hpp"
#include "Util/VKImGUIUtil.hpp"
#include "Libs/IconsFontAwesome5.h"
#include "ImGui/imgui_internal.h"
#include "ComponentMeta/ComponentMetadata.hpp"

namespace worlds {
    EditorSceneView::EditorSceneView(EngineInterfaces interfaces, Editor* ed)
        : interfaces(interfaces)
        , ed(ed) {
        currentWidth = 256;
        currentHeight = 256;
        cam = interfaces.mainCamera;
        recreateRTT();
    }

    void EditorSceneView::drawWindow() {
        static ConVar noScenePad("editor_disableScenePad", "0");

        if (noScenePad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::SetNextWindowSizeConstraints(ImVec2(256.0f, 256.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin(ICON_FA_MAP u8" Scene")) {
            sceneViewPass->active = viewportActive;
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();

            if ((contentRegion.x != currentWidth || contentRegion.y != currentHeight)
                && contentRegion.x > 256 && contentRegion.y > 256) {
                currentWidth  = contentRegion.x;
                currentHeight = contentRegion.y;
                recreateRTT();
            }

            auto wSize = ImGui::GetContentRegionAvail();
            ImGui::Image((ImTextureID)sceneViewDS, ImVec2(currentWidth, currentHeight));

            glm::vec2 wPos = glm::vec2(ImGui::GetWindowPos()) + glm::vec2(ImGui::GetCursorStartPos());
            glm::vec2 mPos = ImGui::GetIO().MousePos;
            glm::vec2 localMPos = mPos - wPos;
            entt::registry& reg = ed->reg;

            entt::entity selectedEntity = ed->currentSelectedEntity;

            if (reg.valid(ed->currentSelectedEntity)) {
                auto& selectedTransform = reg.get<Transform>(ed->currentSelectedEntity);
                auto& t = ed->handleOverriden ? *ed->overrideTransform : selectedTransform;
                ed->handleTools(t, wPos, wSize);

                if (interfaces.inputManager->ctrlHeld() &&
                    interfaces.inputManager->keyPressed(SDL_SCANCODE_D) &&
                    !interfaces.inputManager->mouseButtonHeld(MouseButton::Right, true)) {
                    if (reg.valid(ed->currentSelectedEntity)) {
                        auto newEnt = reg.create();

                        for (auto& ed : ComponentMetadataManager::sorted) {
                            std::array<ENTT_ID_TYPE, 1> t { ed->getComponentID() };
                            auto rtView = reg.runtime_view(t.begin(), t.end());
                            if (!rtView.contains(selectedEntity))
                                continue;

                            ed->clone(selectedEntity, newEnt, reg);
                        }

                        ed->select(newEnt);
                        ed->activateTool(Tool::Translate);

                        slib::List<entt::entity> multiSelectEnts;
                        slib::List<entt::entity> tempEnts = ed->selectedEntities;

                        for (auto ent : ed->selectedEntities) {
                            auto newMultiEnt = reg.create();

                            for (auto& ed : ComponentMetadataManager::sorted) {
                                std::array<ENTT_ID_TYPE, 1> t { ed->getComponentID() };
                                auto rtView = reg.runtime_view(t.begin(), t.end());
                                if (!rtView.contains(ent))
                                    continue;

                                ed->clone(ent, newMultiEnt, reg);
                            }

                            multiSelectEnts.add(newMultiEnt);
                        }

                        for (auto ent : tempEnts) {
                            ed->multiSelect(ent);
                        }

                        for (auto ent : multiSelectEnts) {
                            ed->multiSelect(ent);
                        }
                        ed->undo.pushState(reg);
                    }
                }

                if (interfaces.inputManager->keyPressed(SDL_SCANCODE_DELETE)) {
                    ed->activateTool(Tool::None);
                    reg.destroy(ed->currentSelectedEntity);
                    ed->currentSelectedEntity = entt::null;
                    ed->undo.pushState(reg);

                    for (auto ent : ed->selectedEntities) {
                        reg.destroy(ent);
                    }

                    ed->selectedEntities.clear();
                }
            }

            if (ImGui::IsWindowHovered() && !ImGuizmo::IsUsing()) {
                if (interfaces.inputManager->mouseButtonPressed(MouseButton::Left, true)) {
                    sceneViewPass->requestPick((int)localMPos.x, (int)localMPos.y);
                }

                uint32_t picked;
                if (sceneViewPass->getPickResult(&picked)) {
                    if (picked == UINT32_MAX)
                        picked = entt::null;

                    if (!interfaces.inputManager->shiftHeld()) {
                        ed->select((entt::entity)picked);
                    } else {
                        ed->multiSelect((entt::entity)picked);
                    }
                }
            }
        } else {
            sceneViewPass->active = false;
        }
        ImGui::End();

        if (noScenePad)
            ImGui::PopStyleVar();
    }

    void EditorSceneView::recreateRTT() {
        auto vkCtx = interfaces.renderer->getHandles();

        if (sceneViewPass)
            interfaces.renderer->destroyRTTPass(sceneViewPass);

        RTTPassCreateInfo sceneViewPassCI {
            .cam = cam,
            .width = currentWidth,
            .height = currentHeight,
            .isVr = false,
            .useForPicking = true,
            .enableShadows = shadowsEnabled,
            .outputToScreen = false
        };

        sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);

        if (sceneViewDS)
            vkCtx->device.freeDescriptorSets(vkCtx->descriptorPool, { sceneViewDS });

        sceneViewDS = VKImGUIUtil::createDescriptorSetFor(
            sceneViewPass->sdrFinalTarget->image, vkCtx);
        sceneViewPass->active = true;
    }

    void EditorSceneView::setShadowsEnabled(bool enabled) {
        shadowsEnabled = enabled;
        recreateRTT();
    }

    void EditorSceneView::setViewportActive(bool active) {
        viewportActive = active;
        sceneViewPass->active = active;
    }

    EditorSceneView::~EditorSceneView() {
        auto vkCtx = interfaces.renderer->getHandles();
        vkCtx->device.freeDescriptorSets(vkCtx->descriptorPool, { sceneViewDS });
    }
}
