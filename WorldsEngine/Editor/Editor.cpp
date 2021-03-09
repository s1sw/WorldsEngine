#include "Editor.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "GuiUtil.hpp"
#include "../ComponentMeta/ComponentMetadata.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../ImGui/imgui_internal.h"
#include "../Physics/PhysicsActor.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include "../Serialization/SceneSerialization.hpp"
#include "../Physics/Physics.hpp"
#include "../Core/Log.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/ImGuizmo.h"
#include <filesystem>
#include "../Render/Loaders/SourceModelLoader.hpp"
#include "../Core/NameComponent.hpp"
#include "EditorWindows/EditorWindows.hpp"
#include "../Util/CreateModelObject.hpp"
#include "../Util/VKImGUIUtil.hpp"
#include "../Libs/IconsFontAwesome5.h"
#include "../Libs/IconsFontaudio.h"
#include "../ComponentMeta/ComponentFuncs.hpp"
#include "../Physics/D6Joint.hpp"
#undef near
#undef far

namespace worlds {
    std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> ComponentMetadataManager::metadata;
    std::vector<ComponentEditor*> ComponentMetadataManager::sorted;
    std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> ComponentMetadataManager::bySerializedID;

    const char* toolStr(Tool tool) {
        switch (tool) {
        case Tool::None:
            return "None";
        case Tool::Rotate:
            return "Rotate";
        case Tool::Scale:
            return "Scale";
        case Tool::Translate:
            return "Translate";
        case Tool::Bounds:
            return "Bounds";
        default:
            return "???";
        }
    }

    std::string Editor::generateWindowTitle() {
        return "Worlds Engine Editor | " + interfaces.engine->getCurrentSceneInfo().name;
    }

    void Editor::updateWindowTitle() {
        auto newTitle = generateWindowTitle();
        SDL_SetWindowTitle(interfaces.engine->getMainWindow(), newTitle.c_str());
    }

    Editor::Editor(entt::registry& reg, EngineInterfaces interfaces)
        : currentTool(Tool::Translate)
        , reg(reg)
        , currentSelectedEntity(entt::null)
        , cam(*interfaces.mainCamera)
        , lookX(0.0f)
        , lookY(0.0f)
        , cameraSpeed(5.0f)
        , imguiMetricsOpen(false)
        , active(true)
        , settings()
        , interfaces(interfaces)
        , inputManager(*interfaces.inputManager) {
        texMan = new UITextureManager{interfaces.renderer->getVKCtx()};
        ComponentMetadataManager::setupLookup();
        interfaces.engine->pauseSim = true;

        RTTPassCreateInfo sceneViewPassCI;
        sceneViewPassCI.enableShadows = true;
        sceneViewPassCI.width = 1600;
        sceneViewPassCI.height = 900;
        sceneViewPassCI.isVr = false;
        sceneViewPassCI.outputToScreen = false;
        sceneViewPassCI.useForPicking = true;
        sceneViewPassCI.cam = &cam;
        sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);

        auto vkCtx = interfaces.renderer->getVKCtx();

        sceneViewDS = VKImGUIUtil::createDescriptorSetFor(interfaces.renderer->getSDRTarget(sceneViewPass), vkCtx);

#define ADD_EDITOR_WINDOW(type) editorWindows.push_back(std::make_unique<type>(interfaces, this))

        ADD_EDITOR_WINDOW(EntityList);
        ADD_EDITOR_WINDOW(Assets);
        ADD_EDITOR_WINDOW(EntityEditor);
        ADD_EDITOR_WINDOW(GameControls);
        ADD_EDITOR_WINDOW(StyleEditor);
        ADD_EDITOR_WINDOW(AssetDBExplorer);
        ADD_EDITOR_WINDOW(MaterialEditor);
        ADD_EDITOR_WINDOW(AboutWindow);
        ADD_EDITOR_WINDOW(BakingWindow);
        ADD_EDITOR_WINDOW(SceneSettingsWindow);

#undef ADD_EDITOR_WINDOW
    }

#undef REGISTER_COMPONENT_TYPE

    void Editor::select(entt::entity entity) {
        // Remove selection from existing entity
        if (reg.valid(currentSelectedEntity) && reg.has<UseWireframe>(currentSelectedEntity)) {
            reg.remove<UseWireframe>(currentSelectedEntity);
        }

        currentSelectedEntity = entity;
        // A null entity means we should deselect the current entity
        if (!reg.valid(entity)) return;

        reg.emplace<UseWireframe>(currentSelectedEntity);
    }

    ImVec2 convVec(glm::vec2 gVec) {
        return ImVec2(gVec.x, gVec.y);
    }

    glm::vec2 convVec(ImVec2 iVec) {
        return glm::vec2(iVec.x, iVec.y);
    }

    glm::vec2 worldToScreenG(glm::vec3 wPos, glm::mat4 vp) {
        glm::vec4 preDivPos = vp * glm::vec4(wPos, 1.0f);
        glm::vec2 screenPos = glm::vec2(preDivPos) / preDivPos.w;

        screenPos += 1.0f;
        screenPos *= 0.5f;
        screenPos *= windowSize;
        screenPos.y = windowSize.y - screenPos.y;

        return screenPos;
    }

    ImVec2 worldToScreen(glm::vec3 wPos, glm::mat4 vp) {
        glm::vec2 screenPos = worldToScreenG(wPos, vp);
        return ImVec2(screenPos.x, screenPos.y);
    }

    // Guess roughly how many circle segments we'll need for a circle
    // of the specified radius
    int getCircleSegments(float radius) {
        return glm::max((int)glm::pow(radius, 0.8f), 6);
    }

    void Editor::updateCamera(float deltaTime) {
        if (ImGuizmo::IsUsing()) return;
        float moveSpeed = cameraSpeed;

        static int origMouseX, origMouseY = 0;

        if (inputManager.mouseButtonPressed(MouseButton::Right, true)) {
            SDL_GetMouseState(&origMouseX, &origMouseY);
            inputManager.captureMouse(true);
        } else if (inputManager.mouseButtonReleased(MouseButton::Right, true)) {
            inputManager.captureMouse(false);
        }

        if (inputManager.mouseButtonHeld(MouseButton::Right, true)) {
            // Camera movement
            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT))
                moveSpeed *= 2.0f;

            cameraSpeed += ImGui::GetIO().MouseWheel * 0.5f;

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
                lookX += (float)(inputManager.getMouseDelta().x - warpAmount.x) * 0.005f;
                lookY += (float)(inputManager.getMouseDelta().y - warpAmount.y) * 0.005f;

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
    }

    void Editor::activateTool(Tool newTool) {
        assert(reg.valid(currentSelectedEntity));
        currentTool = newTool;
        originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
        logMsg(SDL_LOG_PRIORITY_DEBUG, "activateTool(%s)", toolStr(newTool));
    }

    void Editor::setActive(bool active) {
        this->active = active;

        if (active) {
            updateWindowTitle();
        }
    }

    template <typename T>
    void copyComponent(entt::entity oldEnt, entt::entity newEnt, entt::registry& reg) {
        if (reg.has<T>(oldEnt))
            reg.emplace<T>(newEnt, reg.get<T>(oldEnt));
    }

    ImGuizmo::OPERATION toolToOp(Tool t) {
        switch (t) {
        default:
        case Tool::Bounds:
        case Tool::None:
            return ImGuizmo::OPERATION::BOUNDS;
        case Tool::Rotate:
            return ImGuizmo::OPERATION::ROTATE;
        case Tool::Translate:
            return ImGuizmo::OPERATION::TRANSLATE;
        case Tool::Scale:
            return ImGuizmo::OPERATION::SCALE;
        }
    }

    void Editor::update(float deltaTime) {
        interfaces.renderer->setRTTPassActive(sceneViewPass, active);

        if (!active) {
            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && inputManager.ctrlHeld() && !inputManager.shiftHeld())
                g_console->executeCommandStr("reloadAndEdit");

            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && inputManager.ctrlHeld() && inputManager.shiftHeld())
                g_console->executeCommandStr("pauseAndEdit");
            return;
        }

        if (reg.valid(currentSelectedEntity)) {
            // Right mouse button means that the view's being moved, so we'll ignore any tools
            // and assume the user's trying to move the camera
            if (!inputManager.mouseButtonHeld(MouseButton::Right, true)) {
                if (inputManager.keyPressed(SDL_SCANCODE_G)) {
                    activateTool(Tool::Translate);
                } else if (inputManager.keyPressed(SDL_SCANCODE_R)) {
                    activateTool(Tool::Rotate);
                } else if (inputManager.keyPressed(SDL_SCANCODE_S)) {
                    activateTool(Tool::Scale);
                } else if (inputManager.keyPressed(SDL_SCANCODE_B)) {
                    activateTool(Tool::Bounds);
                }
            }
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                for (auto& window : editorWindows) {
                    if (window->menuSection() == EditorMenu::File) {
                        if (ImGui::MenuItem(window->getName())) {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                if (ImGui::MenuItem("Quit")) {
                    interfaces.engine->quit();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                for (auto& window : editorWindows) {
                    if (window->menuSection() == EditorMenu::Edit) {
                        if (ImGui::MenuItem(window->getName())) {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window")) {
                for (auto& window : editorWindows) {
                    if (window->menuSection() == EditorMenu::Window) {
                        if (ImGui::MenuItem(window->getName())) {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                for (auto& window : editorWindows) {
                    if (window->menuSection() == EditorMenu::Help) {
                        if (ImGui::MenuItem(window->getName())) {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin(ICON_FA_EDIT u8" Editor")) {
            ImGui::Text("Current tool: %s", toolStr(currentTool));

            ImGui::Checkbox("Manipulate in local space", &toolLocalSpace);

            ImGui::Checkbox("Global object snap", &settings.objectSnapGlobal);
            tooltipHover("If this is checked, moving an object with Ctrl held will snap in increments relative to the world rather than the object's original position.");
            ImGui::Checkbox("Pause physics", &interfaces.engine->pauseSim);
            ImGui::InputFloat("Snap increment", &settings.snapIncrement, 0.1f, 0.5f);
            ImGui::InputFloat("Angular snap increment", &settings.angularSnapIncrement, 0.5f, 1.0f);
        }
        ImGui::End();

        updateCamera(deltaTime);

        static ImVec2 currentSceneViewSize = ImVec2(0.0f, 0.0f);

        static ConVar noScenePad("editor_disableScenePad", "0");

        if (noScenePad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::SetNextWindowSizeConstraints(ImVec2(256.0f, 256.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin(ICON_FA_MAP u8" Scene")) {
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();

            if ((contentRegion.x != currentSceneViewSize.x || contentRegion.y != currentSceneViewSize.y)
                && contentRegion.x > 256 && contentRegion.y > 256) {
                auto vkCtx = interfaces.renderer->getVKCtx();
                // resize!
                currentSceneViewSize = contentRegion;
                interfaces.renderer->destroyRTTPass(sceneViewPass);

                RTTPassCreateInfo sceneViewPassCI;
                sceneViewPassCI.enableShadows = true;
                sceneViewPassCI.width = contentRegion.x;
                sceneViewPassCI.height = contentRegion.y;
                sceneViewPassCI.isVr = false;
                sceneViewPassCI.outputToScreen = false;
                sceneViewPassCI.useForPicking = true;
                sceneViewPassCI.cam = &cam;
                sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);
                vkCtx.device.freeDescriptorSets(vkCtx.descriptorPool, { sceneViewDS });

                sceneViewDS = VKImGUIUtil::createDescriptorSetFor(interfaces.renderer->getSDRTarget(sceneViewPass), vkCtx);
            }
            auto wSize = ImGui::GetContentRegionAvail();

            ImGui::Image((ImTextureID)sceneViewDS, ImVec2(currentSceneViewSize.x, currentSceneViewSize.y));

            auto wPos = ImGui::GetWindowPos() + ImGui::GetCursorStartPos();
            auto mPos = ImGui::GetIO().MousePos;
            auto localMPos = mPos - wPos;

            if (reg.valid(currentSelectedEntity)) {
                auto& selectedTransform = reg.get<Transform>(currentSelectedEntity);
                // Convert selected transform position from world space to screen space
                glm::vec4 ndcObjPosPreDivide = cam.getProjectionMatrix((float)wSize.x / wSize.y) * cam.getViewMatrix() * glm::vec4(selectedTransform.position, 1.0f);

                // NDC -> screen space
                glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
                ndcObjectPosition /= ndcObjPosPreDivide.w;
                ndcObjectPosition *= 0.5f;
                ndcObjectPosition += 0.5f;
                ndcObjectPosition *= convVec(wSize);
                // Not sure why flipping Y is necessary?
                ndcObjectPosition.y = wSize.y - ndcObjectPosition.y;

                if ((ndcObjPosPreDivide.z / ndcObjPosPreDivide.w) > 0.0f)
                    ImGui::GetWindowDrawList()->AddCircleFilled(convVec(ndcObjectPosition) + wPos, 7.0f, ImColor(0.0f, 0.0f, 0.0f));

                ImGuizmo::BeginFrame();
                ImGuizmo::Enable(true);
                ImGuizmo::SetRect(wPos.x, wPos.y, (float)wSize.x, (float)wSize.y);
                ImGuizmo::SetDrawlist();

                glm::mat4 view = cam.getViewMatrix();
                // We have to explicitly get the non-reversed Z matrix here otherwise ImGuizmo freaks out.
                glm::mat4 proj = cam.getProjectionMatrixZO((float)wSize.x / (float)wSize.y);

                glm::mat4 tfMtx = selectedTransform.getMatrix();
                glm::vec3 snap{ 0.0f };

                if (inputManager.keyHeld(SDL_SCANCODE_LCTRL, true)) {
                    switch (currentTool) {
                    case Tool::Rotate:
                        snap = glm::vec3{ settings.angularSnapIncrement };
                        break;
                    default:
                        snap = glm::vec3{ settings.snapIncrement };
                        break;
                    }
                }

                static float bounds[6] { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                    toolToOp(currentTool), toolLocalSpace ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD,
                    glm::value_ptr(tfMtx), nullptr, glm::value_ptr(snap),
                    currentTool == Tool::Bounds ? bounds : nullptr, glm::value_ptr(snap));
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(tfMtx, scale, rotation, translation, skew, perspective);

                static bool usingLast = false;
                if (!usingLast && ImGuizmo::IsUsing()) {
                    undo.pushState(reg);
                }

                usingLast = ImGuizmo::IsUsing();

                switch (currentTool) {
                case Tool::Translate:
                    selectedTransform.position = translation;
                    break;
                case Tool::Rotate:
                    selectedTransform.rotation = rotation;
                    break;
                case Tool::Scale:
                    selectedTransform.scale = scale;
                    break;
                case Tool::Bounds:
                    selectedTransform.position = translation;
                    selectedTransform.rotation = rotation;
                    selectedTransform.scale = scale;
                    break;
                }

                if (inputManager.ctrlHeld() &&
                    inputManager.keyPressed(SDL_SCANCODE_D) &&
                    !inputManager.mouseButtonHeld(MouseButton::Right, true)) {
                    auto newEnt = reg.create();

                    for (auto& ed : ComponentMetadataManager::sorted) {
                        std::array<ENTT_ID_TYPE, 1> t { ed->getComponentID() };
                        auto rtView = reg.runtime_view(t.begin(), t.end());
                        if (!rtView.contains(currentSelectedEntity))
                            continue;

                        ed->clone(currentSelectedEntity, newEnt, reg);
                    }

                    select(newEnt);
                    activateTool(Tool::Translate);
                    undo.pushState(reg);
                }

                if (inputManager.keyPressed(SDL_SCANCODE_DELETE)) {
                    activateTool(Tool::None);
                    reg.destroy(currentSelectedEntity);
                    currentSelectedEntity = entt::null;
                    undo.pushState(reg);
                }
            }

            if (ImGui::IsWindowHovered() && !ImGuizmo::IsUsing()) {
                if (inputManager.mouseButtonPressed(MouseButton::Left, true)) {
                    interfaces.renderer->requestEntityPick((int)localMPos.x, (int)localMPos.y);
                }

                entt::entity picked;
                if (interfaces.renderer->getPickedEnt(&picked)) {
                    if ((uint32_t)picked == UINT32_MAX)
                        picked = entt::null;

                    select(picked);
                }
            }
        }
        ImGui::End();

        if (noScenePad)
            ImGui::PopStyleVar();

        for (auto& edWindow : editorWindows) {
            if (edWindow->isActive()) {
                edWindow->draw(reg);
            }
        }

        if (inputManager.keyPressed(SDL_SCANCODE_S) && inputManager.ctrlHeld()) {
            if (interfaces.engine->getCurrentSceneInfo().id != ~0u && !inputManager.shiftHeld()) {
                saveScene(interfaces.engine->getCurrentSceneInfo().id, reg);
            } else {
                ImGui::OpenPopup("Save Scene");
            }
        }

        saveFileModal("Save Scene", [this](const char* path) {
            AssetID sceneId = g_assetDB.addOrGetExisting(path);
            saveScene(g_assetDB.createAsset(path), reg);
            updateWindowTitle();
        });


        if (inputManager.keyPressed(SDL_SCANCODE_O) && inputManager.ctrlHeld()) {
            ImGui::OpenPopup("Open Scene");
        }

        if (inputManager.keyPressed(SDL_SCANCODE_Z) && inputManager.ctrlHeld()) {
            if (inputManager.shiftHeld()) {
                undo.redo(reg);
            } else {
                undo.undo(reg);
            }
        }

        const char* sceneFileExts[2] = { ".escn", ".wscn" };

        openFileModal("Open Scene", [this](const char* path) {
            reg.clear();
            interfaces.engine->loadScene(g_assetDB.addOrGetExisting(path));
            updateWindowTitle();
            undo.clear();
            }, sceneFileExts, 2);

        if (inputManager.keyPressed(SDL_SCANCODE_I, true) &&
            inputManager.ctrlHeld() &&
            inputManager.shiftHeld()) {
            imguiMetricsOpen = !imguiMetricsOpen;
        }

        if (inputManager.keyPressed(SDL_SCANCODE_P, true) &&
            inputManager.ctrlHeld()) {
            interfaces.engine->pauseSim = false;
            g_console->executeCommandStr("play");
        }

        if (inputManager.keyPressed(SDL_SCANCODE_P, true) &&
            inputManager.ctrlHeld() &&
            inputManager.shiftHeld()) {
            g_console->executeCommandStr("unpause");
        }

        if (imguiMetricsOpen)
            ImGui::ShowMetricsWindow(&imguiMetricsOpen);
    }
}
