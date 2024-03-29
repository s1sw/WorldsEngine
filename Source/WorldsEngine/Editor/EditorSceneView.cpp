#include "Editor.hpp"
#include "Editor/EditorActions.hpp"
#include "ImGui/ImGuizmo.h"
#include "Libs/IconsFontAwesome5.h"
#include "Render/RenderInternal.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ComponentMeta/ComponentMetadata.hpp"
#include "ImGui/imgui_internal.h"
#include "Scripting/NetVM.hpp"
#include <Audio/Audio.hpp>

namespace worlds
{
    struct ComponentIcon
    {
        uint32_t typeId;
        const char* iconPath;
    };

    ComponentIcon icons[] = {
            { entt::type_id<AudioSource>().hash(), "UI/Editor/Images/Audio Source.png" },
            { entt::type_id<WorldLight>().hash(), "UI/Editor/Images/WorldLight.png" },
            { entt::type_id<WorldCubemap>().hash(), "UI/Editor/Images/Cubemap.png" }
    };

    EditorSceneView::EditorSceneView(EngineInterfaces interfaces, Editor* ed) : interfaces(interfaces), ed(ed)
    {
        currentWidth = 256;
        currentHeight = 256;
        cam = *interfaces.mainCamera;
        recreateRTT();
    }

    glm::vec2 convertPositionToScreenSpace(glm::vec2 viewportSize, glm::vec3 position, glm::mat4 vp, bool* behind)
    {
        glm::vec4 ndcObjPosPreDivide = vp * glm::vec4(position, 1.0f);

        glm::vec2 ndcObjectPosition = ndcObjPosPreDivide;
        ndcObjectPosition /= ndcObjPosPreDivide.w;
        ndcObjectPosition *= 0.5f;
        ndcObjectPosition += 0.5f;
        ndcObjectPosition *= (glm::vec2)viewportSize;
        // Not sure why flipping Y is necessary?
        ndcObjectPosition.y = viewportSize.y - ndcObjectPosition.y;

        *behind = (ndcObjPosPreDivide.z / ndcObjPosPreDivide.w) > 0.0f;

        return ndcObjectPosition;
    }

    void EditorSceneView::drawWindow(int uniqueId)
    {
        static ConVar noScenePad("editor_disableScenePad", "0");

        if (noScenePad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::SetNextWindowSizeConstraints(ImVec2(256.0f, 256.0f), ImVec2(FLT_MAX, FLT_MAX));
        std::string windowTitle = std::string((char*)ICON_FA_MAP) + " Scene##" + std::to_string(uniqueId);
        if (ImGui::Begin(windowTitle.c_str(), &open))
        {
            isSeparateWindow = ImGui::GetWindowViewport() != ImGui::GetMainViewport();
            sceneViewPass->active = viewportActive;
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();

            if ((contentRegion.x != currentWidth || contentRegion.y != currentHeight) && contentRegion.x > 256 &&
                contentRegion.y > 256)
            {
                currentWidth = contentRegion.x;
                currentHeight = contentRegion.y;
                sceneViewPass->resize(currentWidth, currentHeight);
            }
            cam.verticalFOV = interfaces.mainCamera->verticalFOV;

            ImGui::Image(sceneViewPass->getUITextureID(), ImVec2(currentWidth, currentHeight));

            ImGui::SetCursorPos(ImGui::GetCursorStartPos());

            const float ANIM_DURATION = 0.1f;
            const float ANIM_SPEED = 1.0f / ANIM_DURATION;

            bool mouseOverToggleArea = false;
            ImVec2 mousePos = ImGui::GetMousePos();

            mouseOverToggleArea = mousePos.x > ImGui::GetCursorScreenPos().x &&
                                  mousePos.y > ImGui::GetCursorScreenPos().y &&
                                  mousePos.y < ImGui::GetCursorScreenPos().y + 50.0f &&
                                  mousePos.x < ImGui::GetCursorScreenPos().x + contentRegion.x;

            glm::vec2 wPos = glm::vec2(ImGui::GetWindowPos()) + glm::vec2(ImGui::GetCursorStartPos());
            glm::vec2 mPos = ImGui::GetIO().MousePos;
            glm::vec2 localMPos = mPos - wPos;
            entt::registry& reg = ed->reg;

            entt::entity selectedEntity = ed->currentSelectedEntity;

            if (ed->handleOverriden)
            {
                ed->handleTools(*ed->overrideTransform, wPos, contentRegion, cam);
            }
            else if (ed->handleOverrideEntity != entt::null)
            {
                if (reg.valid(ed->handleOverrideEntity))
                {
                    auto& t = reg.get<Transform>(ed->handleOverrideEntity);
                    ed->handleTools(t, wPos, contentRegion, cam);
                }
            }
            else if (reg.valid(ed->currentSelectedEntity))
            {
                auto& selectedTransform = reg.get<Transform>(ed->currentSelectedEntity);
                ed->handleTools(selectedTransform, wPos, contentRegion, cam);

                ChildComponent* childComponent = reg.try_get<ChildComponent>(ed->currentSelectedEntity);
                if (childComponent)
                {
                    if (reg.valid(childComponent->parent))
                    {
                        Transform& parentTransform = reg.get<Transform>(childComponent->parent);

                        // preserve scale!!!
                        glm::vec3 scale = childComponent->offset.scale;
                        childComponent->offset = selectedTransform.transformByInverse(parentTransform);
                        childComponent->offset.scale = scale;
                    }
                }

                if (interfaces.inputManager->ctrlHeld() && interfaces.inputManager->keyPressed(SDL_SCANCODE_D) &&
                    !interfaces.inputManager->mouseButtonHeld(MouseButton::Right, true))
                {
                    if (reg.valid(ed->currentSelectedEntity))
                    {
                        auto newEnt = reg.create();

                        for (auto& ed : ComponentMetadataManager::sorted)
                        {
                            std::array<ENTT_ID_TYPE, 1> t{ed->getComponentID()};
                            auto rtView = reg.runtime_view(t.begin(), t.end());
                            if (!rtView.contains(selectedEntity))
                                continue;

                            ed->clone(selectedEntity, newEnt, reg);
                        }

                        interfaces.scriptEngine->copyManagedComponents(selectedEntity, newEnt);

                        slib::List<entt::entity> multiSelectEnts;
                        slib::List<entt::entity> tempEnts = ed->selectedEntities;

                        ed->select(newEnt);
                        ed->activateTool(Tool::Translate);

                        for (auto ent : tempEnts)
                        {
                            auto newMultiEnt = reg.create();

                            for (auto& ed : ComponentMetadataManager::sorted)
                            {
                                std::array<ENTT_ID_TYPE, 1> t{ed->getComponentID()};
                                auto rtView = reg.runtime_view(t.begin(), t.end());
                                if (!rtView.contains(ent))
                                    continue;

                                ed->clone(ent, newMultiEnt, reg);
                            }

                            interfaces.scriptEngine->copyManagedComponents(ent, newMultiEnt);

                            multiSelectEnts.add(newMultiEnt);
                        }

                        for (auto ent : multiSelectEnts)
                        {
                            ed->multiSelect(ent);
                        }
                        ed->undo.pushState(reg);
                    }
                }

                if (interfaces.inputManager->keyPressed(SDL_SCANCODE_DELETE))
                {
                    ed->activateTool(Tool::None);
                    reg.destroy(ed->currentSelectedEntity);
                    ed->currentSelectedEntity = entt::null;
                    ed->undo.pushState(reg);

                    for (auto ent : ed->selectedEntities)
                    {
                        reg.destroy(ent);
                    }

                    ed->selectedEntities.clear();
                }
            }

            updateCamera(ImGui::GetIO().DeltaTime);

            ImGuiStyle& style = ImGui::GetStyle();
            const ImColor popupBg = style.Colors[ImGuiCol_WindowBg];

            glm::mat4 proj = cam.getProjectionMatrix(contentRegion.x / contentRegion.y);
            glm::mat4 view = cam.getViewMatrix();
            glm::mat4 vp = proj * view;
            reg.view<EditorLabel, Transform>().each([&](EditorLabel& label, Transform& t) {
                bool behind;
                glm::vec2 screenPos = convertPositionToScreenSpace(contentRegion, t.position, vp, &behind);

                float distanceToObj = glm::distance(t.position, cam.position);

                if (behind && distanceToObj < ed->settings.sceneIconDrawDistance)
                {
                    glm::vec2 textSize = ImGui::CalcTextSize(label.label.cStr());
                    glm::vec2 drawPos = screenPos + wPos - (textSize * 0.5f);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    drawList->AddRectFilled(drawPos - glm::vec2(5.0f, 2.0f), drawPos + textSize + glm::vec2(5.0f, 2.0f),
                                            popupBg, 7.0f);
                    drawList->AddText(drawPos, ImColor(1.0f, 1.0f, 1.0f), label.label.cStr());
                }
            });

            bool mouseOverIcon = false;

            for (ComponentIcon& icon : icons) {
                std::array<uint32_t, 1> t{icon.typeId};
                reg.runtime_view(t.begin(), t.end()).each([&](entt::entity e) {
                    const auto& t = reg.get<Transform>(e);

                    bool behind;
                    glm::vec2 screenPos = convertPositionToScreenSpace(contentRegion, t.position, vp, &behind);

                    float distanceToObj = glm::distance(t.position, cam.position);

                    if (behind && distanceToObj < ed->settings.sceneIconDrawDistance)
                    {
                        glm::vec2 imgSize{64.0f, 64.0f};
                        glm::vec2 drawPos = screenPos + wPos - (imgSize * 0.5f);

                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        IUITextureManager* texMan = interfaces.renderer->getUITextureManager();
                        ImTextureID iconId = texMan->loadOrGet(AssetDB::pathToId(icon.iconPath));
                        drawList->AddImage(iconId, drawPos, drawPos + imgSize);
                        if (ImGui::IsMouseHoveringRect(drawPos, drawPos + imgSize))
                        {
                            mouseOverIcon = true;

                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
                                ed->select(e);
                        }
                    }
                });
            }

            if (ImGui::IsWindowHovered() && !ImGuizmo::IsUsing() && !mouseOverIcon)
            {
                static bool pickRequested = false;
                if (interfaces.inputManager->mouseButtonPressed(MouseButton::Left, true))
                {
                    //sceneViewPass->requestPick((int)localMPos.x, (int)localMPos.y);
                    PickParams params{};
                    params.cam = &cam;
                    params.pickX = localMPos.x;
                    params.pickY = localMPos.y;
                    params.screenWidth = sceneViewPass->width;
                    params.screenHeight = sceneViewPass->height;
                    interfaces.renderer->requestPick(params);
                    pickRequested = true;
                }

                uint32_t picked;
                if (pickRequested && interfaces.renderer->getPickResult(picked))
                {
                    if (picked == UINT32_MAX)
                        picked = entt::null;

                    if (ed->entityEyedropperActive)
                    {
                        ed->eyedropperSelect((entt::entity)picked);
                    }
                    else if (!interfaces.inputManager->shiftHeld())
                    {
                        ed->select((entt::entity)picked);
                    }
                    else
                    {
                        ed->multiSelect((entt::entity)picked);
                    }
                    pickRequested = false;
                }
            }
        }
        else
        {
            sceneViewPass->active = false;
        }
        ImGui::End();

        if (noScenePad)
            ImGui::PopStyleVar();
    }

    void EditorSceneView::recreateRTT()
    {
        if (sceneViewPass)
            interfaces.renderer->destroyRTTPass(sceneViewPass);

        RTTPassSettings sceneViewPassCI
        {
            .cam = &cam,
            .width = currentWidth,
            .height = currentHeight,
            .useForPicking = true,
            .enableShadows = shadowsEnabled,
            .msaaLevel = 4
        };

        sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);
        sceneViewPass->active = true;
    }

    void EditorSceneView::setShadowsEnabled(bool enabled)
    {
        shadowsEnabled = enabled;
        recreateRTT();
    }

    void EditorSceneView::setViewportActive(bool active)
    {
        viewportActive = active;
        sceneViewPass->active = active;
    }

    Camera& EditorSceneView::getCamera()
    {
        return cam;
    }

    void EditorSceneView::updateCamera(float deltaTime)
    {
        if (ImGuizmo::IsUsing())
            return;
        if (!ImGui::IsWindowHovered())
            return;
        float moveSpeed = ed->cameraSpeed;

        static int origMouseX, origMouseY = 0;

        InputManager& inputManager = *interfaces.inputManager;

        if (inputManager.mouseButtonPressed(MouseButton::Right, true))
        {
            SDL_GetMouseState(&origMouseX, &origMouseY);
            inputManager.captureMouse(true);
            ImGui::SetWindowFocus();
        }
        else if (inputManager.mouseButtonReleased(MouseButton::Right, true))
        {
            inputManager.captureMouse(false);
        }

        if (inputManager.mouseButtonHeld(MouseButton::Right, true))
        {
            // Camera movement
            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT))
                moveSpeed *= 2.0f;

            EditorActions::disableForThisFrame();

            float linearisedCamSpeed = log2f(ed->cameraSpeed);
            linearisedCamSpeed += ImGui::GetIO().MouseWheel * 0.1f;
            ed->cameraSpeed = powf(2.0f, linearisedCamSpeed);

            if (inputManager.keyHeld(SDL_SCANCODE_W))
            {
                cam.position += cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_S))
            {
                cam.position -= cam.rotation * glm::vec3(0.0f, 0.0f, deltaTime * moveSpeed);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_A))
            {
                cam.position += cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_D))
            {
                cam.position -= cam.rotation * glm::vec3(deltaTime * moveSpeed, 0.0f, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_SPACE))
            {
                cam.position += cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
            }

            if (inputManager.keyHeld(SDL_SCANCODE_LCTRL))
            {
                cam.position -= cam.rotation * glm::vec3(0.0f, deltaTime * moveSpeed, 0.0f);
            }

            // Mouse wrap around
            // If it leaves the screen, teleport it back on the screen but on the opposite side
            auto mousePos = inputManager.getMousePosition();
            static glm::ivec2 warpAmount(0, 0);

            if (!inputManager.mouseButtonPressed(MouseButton::Right))
            {
                lookX += (float)(ImGui::GetIO().MouseDelta.x - warpAmount.x) * 0.005f;
                lookY += (float)(ImGui::GetIO().MouseDelta.y - warpAmount.y) * 0.005f;

                lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

                cam.rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) *
                               glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
            }

            warpAmount = glm::ivec2{0};

            if (mousePos.x > windowSize.x)
            {
                warpAmount = glm::ivec2(-windowSize.x, 0);
                inputManager.warpMouse(glm::ivec2(mousePos.x - windowSize.x, mousePos.y));
            }
            else if (mousePos.x < 0)
            {
                warpAmount = glm::ivec2(windowSize.x, 0);
                inputManager.warpMouse(glm::ivec2(mousePos.x + windowSize.x, mousePos.y));
            }

            if (mousePos.y > windowSize.y)
            {
                warpAmount = glm::ivec2(0, -windowSize.y);
                inputManager.warpMouse(glm::ivec2(mousePos.x, mousePos.y - windowSize.y));
            }
            else if (mousePos.y < 0)
            {
                warpAmount = glm::ivec2(0, windowSize.y);
                inputManager.warpMouse(glm::ivec2(mousePos.x, mousePos.y + windowSize.y));
            }
        }

        if (ed->reg.valid(ed->currentSelectedEntity) && inputManager.keyPressed(SDL_SCANCODE_F))
        {
            auto& t = ed->reg.get<Transform>(ed->currentSelectedEntity);

            glm::vec3 dirVec = glm::normalize(cam.position - t.position);
            float dist = 5.0f;
            cam.position = t.position + dirVec * dist;
            float pitch = asinf(dirVec.y);
            float yaw = atan2f(dirVec.x, dirVec.z);
            lookX = glm::pi<float>() - yaw;
            lookY = pitch;
            cam.rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) *
                           glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
        }
    }

    EditorSceneView::~EditorSceneView()
    {
        interfaces.renderer->destroyRTTPass(sceneViewPass);
    }
}
