#include "Editor.hpp"
#include "ImGui/ImGuizmo.h"
#include "Render/RenderInternal.hpp"
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
        cam = *interfaces.mainCamera;
        recreateRTT();
    }

    void EditorSceneView::drawWindow(int uniqueId) {
        static ConVar noScenePad("editor_disableScenePad", "0");

        if (noScenePad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::SetNextWindowSizeConstraints(ImVec2(256.0f, 256.0f), ImVec2(FLT_MAX, FLT_MAX));
        std::string windowTitle = std::string((char*)ICON_FA_MAP) + " Scene##" + std::to_string(uniqueId);
        if (ImGui::Begin(windowTitle.c_str(), &open)) {
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
                ed->handleTools(t, wPos, wSize, cam);

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
                static bool pickRequested = false;
                if (interfaces.inputManager->mouseButtonPressed(MouseButton::Left, true)) {
                    sceneViewPass->requestPick((int)localMPos.x, (int)localMPos.y);
                    pickRequested = true;
                }

                uint32_t picked;
                if (pickRequested && sceneViewPass->getPickResult(&picked)) {
                    if (picked == UINT32_MAX)
                        picked = entt::null;

                    if (ed->entityEyedropperActive) {
                        ed->eyedropperSelect((entt::entity)picked);
                    } else if (!interfaces.inputManager->shiftHeld()) {
                        ed->select((entt::entity)picked);
                    } else {
                        ed->multiSelect((entt::entity)picked);
                    }
                    pickRequested = false;
                }
            }

            updateCamera(ImGui::GetIO().DeltaTime);

            ImGuiStyle& style = ImGui::GetStyle();
            const ImColor popupBg = style.Colors[ImGuiCol_WindowBg];

            reg.view<EditorLabel, Transform>().each([&](EditorLabel& label, Transform& t) {
                glm::mat4 proj = cam.getProjectionMatrix(contentRegion.x / contentRegion.y);
                glm::mat4 view = cam.getViewMatrix();

                glm::vec4 ndcObjPosPreDivide = proj * view * glm::vec4(t.position, 1.0f);

                glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
                ndcObjectPosition /= ndcObjPosPreDivide.w;
                ndcObjectPosition *= 0.5f;
                ndcObjectPosition += 0.5f;
                ndcObjectPosition *= (glm::vec2)contentRegion;
                // Not sure why flipping Y is necessary?
                ndcObjectPosition.y = wSize.y - ndcObjectPosition.y;

                if ((ndcObjPosPreDivide.z / ndcObjPosPreDivide.w) > 0.0f && glm::distance(t.position, cam.position) < 10.0f) {
                    glm::vec2 textSize = ImGui::CalcTextSize(label.label.c_str());
                    glm::vec2 drawPos = ndcObjectPosition + wPos - (textSize * 0.5f);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    drawList->AddRectFilled(drawPos - glm::vec2(5.0f, 2.0f), drawPos + textSize + glm::vec2(5.0f, 2.0f), popupBg, 7.0f);

                    ImGui::GetWindowDrawList()->AddText(drawPos, ImColor(1.0f, 1.0f, 1.0f), label.label.c_str());
                }

                });
        } else {
            sceneViewPass->active = false;
        }
        ImGui::End();

        if (noScenePad)
            ImGui::PopStyleVar();
    }

    void EditorSceneView::recreateRTT() {
        auto vkCtx = static_cast<VKRenderer*>(interfaces.renderer)->getHandles();

        if (sceneViewPass)
            interfaces.renderer->destroyRTTPass(sceneViewPass);

        RTTPassCreateInfo sceneViewPassCI {
            .cam = &cam,
            .width = currentWidth,
            .height = currentHeight,
            .isVr = false,
            .useForPicking = true,
            .enableShadows = shadowsEnabled,
            .outputToScreen = false
        };

        sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);

        if (sceneViewDS) {
            vkFreeDescriptorSets(vkCtx->device, vkCtx->descriptorPool, 1, &sceneViewDS);
        }

        sceneViewDS = VKImGUIUtil::createDescriptorSetFor(
            static_cast<VKRTTPass*>(sceneViewPass)->sdrFinalTarget->image, vkCtx);
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

    void EditorSceneView::updateCamera(float deltaTime) {
        if (ImGuizmo::IsUsing()) return;
        if (!ImGui::IsWindowHovered()) return;
        float moveSpeed = ed->cameraSpeed;

        static int origMouseX, origMouseY = 0;

        InputManager& inputManager = *interfaces.inputManager;

        if (inputManager.mouseButtonPressed(MouseButton::Right, true)) {
            SDL_GetMouseState(&origMouseX, &origMouseY);
            inputManager.captureMouse(true);
            ImGui::SetWindowFocus();
        } else if (inputManager.mouseButtonReleased(MouseButton::Right, true)) {
            inputManager.captureMouse(false);
        }

        if (inputManager.mouseButtonHeld(MouseButton::Right, true)) {
            // Camera movement
            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT))
                moveSpeed *= 2.0f;

            ed->cameraSpeed += ImGui::GetIO().MouseWheel * 0.5f;

            if (inputManager.keyHeld(SDL_SCANCODE_W)) {
                cam.position += cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_S)) {
                cam.position -= cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_A)) {
                cam.position += cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_D)) {
                cam.position -= cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_SPACE)) {
                cam.position += cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_LCTRL)) {
                cam.position -= cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
            }

            // Mouse wrap around
            // If it leaves the screen, teleport it back on the screen but on the opposite side
            auto mousePos = inputManager.getMousePosition();
            static glm::ivec2 warpAmount(0, 0);

            if (!inputManager.mouseButtonPressed(MouseButton::Right)) {
                lookX += (float)(ImGui::GetIO().MouseDelta.x - warpAmount.x) * 0.005f;
                lookY += (float)(ImGui::GetIO().MouseDelta.y - warpAmount.y) * 0.005f;

                lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

                cam.rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
            }

            warpAmount = glm::ivec2{ 0 };
            
            
            if (mousePos.x > windowSize.x) {
                warpAmount = glm::ivec2(-windowSize.x, 0);
                inputManager.warpMouse(glm::ivec2(mousePos.x - windowSize.x, mousePos.y));
            } else if (mousePos.x < 0) {
                warpAmount = glm::ivec2(windowSize.x, 0);
                inputManager.warpMouse(glm::ivec2(mousePos.x + windowSize.x, mousePos.y));
            }
            
            if (mousePos.y > windowSize.y) {
                warpAmount = glm::ivec2(0, -windowSize.y);
                inputManager.warpMouse(glm::ivec2(mousePos.x, mousePos.y - windowSize.y));
            } else if (mousePos.y < 0) {
                warpAmount = glm::ivec2(0, windowSize.y);
                inputManager.warpMouse(glm::ivec2(mousePos.x, mousePos.y + windowSize.y));
            }
        }

        if (ed->reg.valid(ed->currentSelectedEntity) && inputManager.keyPressed(SDL_SCANCODE_F)) {
            auto& t = ed->reg.get<Transform>(ed->currentSelectedEntity);

            glm::vec3 dirVec = glm::normalize(cam.position - t.position);
            float dist = 5.0f;
            cam.position = t.position + dirVec * dist;
            cam.rotation = glm::quatLookAt(dirVec, glm::vec3{0.0f, 1.0f, 0.0f});
        }
    }

    EditorSceneView::~EditorSceneView() {
        auto vkCtx = static_cast<VKRenderer*>(interfaces.renderer)->getHandles();
        vkFreeDescriptorSets(vkCtx->device, vkCtx->descriptorPool, 1, &sceneViewDS);
        interfaces.renderer->destroyRTTPass(sceneViewPass);
    }
}
