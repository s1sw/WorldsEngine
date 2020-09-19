#include "Editor.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "GuiUtil.hpp"
#include "ComponentMetadata.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS 
#include "imgui_internal.h"
#include "PhysicsActor.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include "ShaderMetadata.hpp"
#include "SceneSerialization.hpp"
#include "Physics.hpp"
#include "Log.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include <filesystem>
#include "SourceModelLoader.hpp"
#undef near
#undef far

namespace worlds {
    extern bool pauseSim;

    physx::PxMaterial* defaultMaterial;

    std::unordered_map<ENTT_ID_TYPE, ComponentMetadata> ComponentMetadataManager::metadata;

    glm::vec3 filterAxes(glm::vec3 vec, AxisFlagBits axisFlags) {
        return vec * glm::vec3(
            (axisFlags & AxisFlagBits::X) == AxisFlagBits::X,
            (axisFlags & AxisFlagBits::Y) == AxisFlagBits::Y,
            (axisFlags & AxisFlagBits::Z) == AxisFlagBits::Z);
    }

    int getNumActiveAxes(AxisFlagBits axisFlags) {
        return
            ((axisFlags & AxisFlagBits::X) == AxisFlagBits::X) +
            ((axisFlags & AxisFlagBits::Y) == AxisFlagBits::Y) +
            ((axisFlags & AxisFlagBits::Z) == AxisFlagBits::Z);
    }

    bool hasAxis(AxisFlagBits flags, AxisFlagBits axis) {
        return (flags & axis) == axis;
    }

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
        default:
            return "???";
        }
    }

#define REGISTER_COMPONENT_TYPE(type, name, showInInspector, editFunc, createFunc, cloneFunc) ComponentMetadataManager::registerMetadata(entt::type_info<type>::id(), ComponentMetadata {name, showInInspector, entt::type_info<type>::id(), editFunc, createFunc, cloneFunc})

    void editTransform(entt::entity ent, entt::registry& reg) {
        ImGui::Text("Transform");
        auto& selectedTransform = reg.get<Transform>(ent);
        ImGui::DragFloat3("Position", &selectedTransform.position.x);

        glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(selectedTransform.rotation));
        if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot))) {
            selectedTransform.rotation = glm::radians(eulerRot);
        }

        ImGui::DragFloat3("Scale", &selectedTransform.scale.x);
        ImGui::Separator();
    }

    void editWorldObject(entt::entity ent, entt::registry& reg) {
        ImGui::Text("WorldObject");
        ImGui::SameLine();
        if (ImGui::Button("Remove##WO")) {
            reg.remove<WorldObject>(ent);
        } else {
            auto& worldObject = reg.get<WorldObject>(ent);
            ImGui::DragFloat2("Texture Scale", &worldObject.texScaleOffset.x);
            ImGui::DragFloat2("Texture Offset", &worldObject.texScaleOffset.z);

            for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                if (worldObject.presentMaterials[i]) {
                    ImGui::Text("Material %i: %s", i, g_assetDB.getAssetPath(worldObject.materials[i]).c_str());
                    
                } else {
                    ImGui::Text("Material %i: not set", i);
                }

                ImGui::SameLine();

                std::string idStr = "##" + std::to_string(i);

                bool open = ImGui::Button(("Change" + idStr).c_str());
                if (selectAssetPopup(("Material" + idStr).c_str(), worldObject.materials[i], open)) {
                    worldObject.materialIdx[i] = ~0u;
                    worldObject.presentMaterials[i] = true;
                }
            }
            ImGui::Separator();
        }
    }

    void createLight(entt::entity ent, entt::registry& reg) {
        reg.emplace<WorldLight>(ent);
    }

    void createPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& t = reg.get<Transform>(ent);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidStatic(pTf);
        reg.emplace<PhysicsActor>(ent, actor);
        g_scene->addActor(*actor);
    }

    void createDynamicPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& t = reg.get<Transform>(ent);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidDynamic(pTf);
        reg.emplace<DynamicPhysicsActor>(ent, actor);
        g_scene->addActor(*actor);
    }

    const const char* shapeTypeNames[(int)PhysicsShapeType::Count] = {
        "Sphere",
        "Box",
        "Capsule",
        "Mesh"
    };

    template <typename T>
    void editPhysicsShapes(T& actor) {
        ImGui::Text("Shapes: %i", actor.physicsShapes.size());

        ImGui::SameLine();

        if (ImGui::Button("Add")) {
            actor.physicsShapes.push_back(PhysicsShape::boxShape(glm::vec3(0.5f)));
        }

        std::vector<PhysicsShape>::iterator eraseIter;
        bool erase = false;

        for (auto it = actor.physicsShapes.begin(); it != actor.physicsShapes.end(); it++) {
            if (ImGui::BeginCombo("Collider Type", shapeTypeNames[(int)it->type])) {
                for (int iType = 0; iType < (int)PhysicsShapeType::Count; iType++) {
                    auto type = (PhysicsShapeType)iType;
                    bool isSelected = it->type == type;
                    if (ImGui::Selectable(shapeTypeNames[iType], &isSelected)) {
                        it->type = type;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                eraseIter = it;
                erase = true;
            }

            switch (it->type) {
            case PhysicsShapeType::Sphere:
                ImGui::DragFloat("Radius", &it->sphere.radius);
                break;
            case PhysicsShapeType::Box:
                ImGui::DragFloat3("Half extents", &it->box.halfExtents.x);
                break;
            case PhysicsShapeType::Capsule:
                ImGui::DragFloat("Height", &it->capsule.height);
                ImGui::DragFloat("Radius", &it->capsule.radius);
                break;
            }
        }

        if (erase)
            actor.physicsShapes.erase(eraseIter);
    }

    void editPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<PhysicsActor>(ent);
        ImGui::Text("Physics Actor");
        ImGui::SameLine();

        if (ImGui::Button("Remove##PA")) {
            reg.remove<PhysicsActor>(ent);
        } else {
            if (ImGui::Button("Update Collisions")) {
                updatePhysicsShapes(pa);
            }

            editPhysicsShapes(pa);
            ImGui::Separator();
        }
    }

    void editDynamicPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<DynamicPhysicsActor>(ent);
        ImGui::Text("Dynamic Physics Actor");
        ImGui::SameLine();

        if (ImGui::Button("Remove##DPA")) {
            reg.remove<DynamicPhysicsActor>(ent);
        } else {
            ImGui::DragFloat("Mass", &pa.mass);
            if (ImGui::Button("Update Collisions##DPA")) {
                updatePhysicsShapes(pa);
                physx::PxRigidBodyExt::updateMassAndInertia(*((physx::PxRigidDynamic*)pa.actor), &pa.mass, 1);
            }

            editPhysicsShapes(pa);
            ImGui::Separator();
        }
    }

    void clonePhysicsActor(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& t = reg.get<Transform>(b);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidStatic(pTf);

        auto& newPhysActor = reg.emplace<PhysicsActor>(b, actor);
        newPhysActor.physicsShapes = reg.get<PhysicsActor>(a).physicsShapes;

        g_scene->addActor(*actor);

        updatePhysicsShapes(newPhysActor);
    }

    void cloneDynamicPhysicsActor(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& t = reg.get<Transform>(b);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidDynamic(pTf);

        auto& newPhysActor = reg.emplace<DynamicPhysicsActor>(b, actor);
        newPhysActor.physicsShapes = reg.get<DynamicPhysicsActor>(a).physicsShapes;

        g_scene->addActor(*actor);

        updatePhysicsShapes(newPhysActor);
    }

    Editor::Editor(entt::registry& reg, InputManager& inputManager, Camera& cam)
        : reg(reg)
        , inputManager(inputManager)
        , cam(cam)
        , currentTool(Tool::None)
        , currentAxisLock(AxisFlagBits::All)
        , lookX(0.0f)
        , lookY(0.0f)
        , settings()
        , imguiMetricsOpen(false)
        , currentSelectedEntity(entt::null)
        , enableTransformGadget(false)
        , startingMouseDistance(0.0f) {
        REGISTER_COMPONENT_TYPE(Transform, "Transform", true, editTransform, nullptr, nullptr);
        REGISTER_COMPONENT_TYPE(WorldObject, "WorldObject", true, editWorldObject, nullptr, nullptr);
        REGISTER_COMPONENT_TYPE(WorldLight, "WorldLight", true, nullptr, createLight, nullptr);
        REGISTER_COMPONENT_TYPE(PhysicsActor, "PhysicsActor", true, editPhysicsActor, createPhysicsActor, clonePhysicsActor);
        REGISTER_COMPONENT_TYPE(DynamicPhysicsActor, "DynamicPhysicsActor", true, editDynamicPhysicsActor, createDynamicPhysicsActor, cloneDynamicPhysicsActor);
        pauseSim = true;
        defaultMaterial = g_physics->createMaterial(0.5f, 0.5f, 0.1f);
    }

#undef REGISTER_COMPONENT_TYPE

    void Editor::select(entt::entity entity) {
        if (currentTool != Tool::None) return;
        // Remove selection from existing entity
        if (reg.valid(currentSelectedEntity)) {
            reg.remove<UseWireframe>(currentSelectedEntity);
        }

        currentSelectedEntity = entity;
        // A null entity means we should deselect the current entity
        if (!reg.valid(entity)) return;

        reg.emplace<UseWireframe>(currentSelectedEntity);
    }

    void Editor::handleAxisButtonPress(AxisFlagBits axisFlagBit) {
        if (currentAxisLock == AxisFlagBits::All)
            currentAxisLock = axisFlagBit;
        else
            currentAxisLock = currentAxisLock ^ axisFlagBit;
    }

    glm::vec3 projectRayToPlane(glm::vec3 origin, glm::vec3 dir, glm::vec3 planeNormal, float d) {
        float t = -(glm::dot(origin, planeNormal) + d) / glm::dot(dir, planeNormal);

        return origin + (dir * t);
    }

    ImVec2 glmToImgui(glm::vec2 gVec) {
        return ImVec2(gVec.x, gVec.y);
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

    // Based on code from the ImGui demo
    void tooltipHover(const char* desc) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void Editor::updateCamera(float deltaTime) {
        if (currentTool != Tool::None) return;
        glm::vec3 prevPos = cam.position;
        float moveSpeed = 5.0f;

        static int origMouseX, origMouseY = 0;

        if (inputManager.mouseButtonPressed(MouseButton::Right)) {
            SDL_GetMouseState(&origMouseX, &origMouseY);
        }

        if (inputManager.mouseButtonHeld(MouseButton::Right)) {
            // Camera movement
            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT))
                moveSpeed *= 2.0f;

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
            glm::ivec2 warpAmount(0, 0);

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

            if (!inputManager.mouseButtonPressed(MouseButton::Right)) {
                lookX += (float)(inputManager.getMouseDelta().x - warpAmount.x) * 0.005f;
                lookY += (float)(inputManager.getMouseDelta().y - warpAmount.y) * 0.005f;

                lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

                cam.rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
            }
        }
    }

    void Editor::activateTool(Tool newTool) {
        assert(reg.valid(currentSelectedEntity));
        currentTool = newTool;
        originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
        currentAxisLock = AxisFlagBits::All;
        startingMouseDistance = -1.0f;
        logMsg(SDL_LOG_PRIORITY_DEBUG, "activateTool(%s)", toolStr(newTool));
    }

    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" }
    };

    template <typename T>
    void copyComponent(entt::entity oldEnt, entt::entity newEnt, entt::registry& reg) {
        if (reg.has<T>(oldEnt))
            reg.emplace<T>(newEnt, reg.get<T>(oldEnt));
    }

    extern entt::entity createModelObject(entt::registry& reg, glm::vec3 position, glm::quat rotation, AssetID meshId, AssetID materialId, glm::vec3 scale = glm::vec3(1.0f), glm::vec4 texScaleOffset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));

    void Editor::update(float deltaTime) {
        if (currentTool == Tool::None && reg.valid(currentSelectedEntity)) {
            // Right mouse button means that the view's being moved, so we'll ignore any tools
            // and assume the user's trying to move the camera
            if (!inputManager.mouseButtonHeld(MouseButton::Right)) {
                if (inputManager.keyPressed(SDL_SCANCODE_G)) {
                    activateTool(Tool::Translate);
                } else if (inputManager.keyPressed(SDL_SCANCODE_R)) {
                    activateTool(Tool::Rotate);
                } else if (inputManager.keyPressed(SDL_SCANCODE_S)) {
                    activateTool(Tool::Scale);
                }
            }
        } else {
            // Complex axis juggling

            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT) || inputManager.keyHeld(SDL_SCANCODE_RSHIFT)) {
                if (inputManager.keyPressed(SDL_SCANCODE_X)) {
                    currentAxisLock = AxisFlagBits::Y | AxisFlagBits::Z;
                } else if (inputManager.keyPressed(SDL_SCANCODE_Y)) {
                    currentAxisLock = AxisFlagBits::X | AxisFlagBits::Z;
                } else if (inputManager.keyPressed(SDL_SCANCODE_Z)) {
                    currentAxisLock = AxisFlagBits::X | AxisFlagBits::Y;
                }
            } else if (inputManager.keyPressed(SDL_SCANCODE_X)) {
                handleAxisButtonPress(AxisFlagBits::X);
            } else if (inputManager.keyPressed(SDL_SCANCODE_Y)) {
                handleAxisButtonPress(AxisFlagBits::Y);
            } else if (inputManager.keyPressed(SDL_SCANCODE_Z)) {
                handleAxisButtonPress(AxisFlagBits::Z);
            }
        }

        if (ImGui::Begin("Editor")) {
            char buf[6];
            char* curr = buf;

            if (hasAxis(currentAxisLock, AxisFlagBits::X)) {
                strcpy(curr, "X,");
                curr += 2;
            }

            if (hasAxis(currentAxisLock, AxisFlagBits::Y)) {
                strcpy(curr, "Y,");
                curr += 2;
            }

            if (hasAxis(currentAxisLock, AxisFlagBits::Z)) {
                strcpy(curr, "Z");
                curr++;
            }

            ImGui::Text("Current axes: %s", buf);
            ImGui::Text("Current tool: %s", toolStr(currentTool));

            ImGui::Checkbox("Global object snap", &settings.objectSnapGlobal);
            tooltipHover("If this is checked, moving an object with Ctrl held will snap in increments relative to the world rather than the object's original position.");
            ImGui::Checkbox("Enable transform gadget", &enableTransformGadget);
            ImGui::Checkbox("Pause physics", &pauseSim);
            ImGui::InputFloat("Scale snap increment", &settings.scaleSnapIncrement, 0.1f, 0.5f);
        }
        ImGui::End();

        if (currentTool != Tool::None) {
            auto& selectedTransform = reg.get<Transform>(currentSelectedEntity);

            if (inputManager.mouseButtonPressed(MouseButton::Right)) {
                selectedTransform = originalObjectTransform;
                currentTool = Tool::None;
            } else if (inputManager.mouseButtonPressed(MouseButton::Left)) {
                currentTool = Tool::None;
            }

            if (currentTool == Tool::Translate) {
                glm::vec2 halfWindowSize = glm::vec2(windowSize) * 0.5f;

                glm::vec2 ndcMousePos = inputManager.getMousePosition();
                ndcMousePos -= halfWindowSize;
                ndcMousePos.x /= halfWindowSize.x;
                ndcMousePos.y /= halfWindowSize.y;
                ndcMousePos *= -1.0f;

                float aspect = (float)windowSize.x / windowSize.y;
                //glm::vec3 dir = (cam.rotation * glm::normalize(glm::vec3(ndcMousePos, 1.0f)));
                float tanHalfFov = glm::tan(cam.verticalFOV * 0.5f);
                glm::vec3 dir(ndcMousePos.x * aspect * tanHalfFov, ndcMousePos.y * tanHalfFov, 1.0f);
                dir = glm::normalize(dir);
                dir = cam.rotation * dir;

                glm::vec3 n;

                if (currentAxisLock == AxisFlagBits::All) {
                    n = cam.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                } else if (getNumActiveAxes(currentAxisLock) == 2) {
                    // Construct a plane along the two active axes
                    glm::vec3 x(1.0f, 0.0f, 0.0f);
                    glm::vec3 y(0.0f, 1.0f, 0.0f);
                    glm::vec3 z(0.0f, 0.0f, 1.0f);

                    x *= 1 - hasAxis(currentAxisLock, AxisFlagBits::X);
                    y *= 1 - hasAxis(currentAxisLock, AxisFlagBits::Y);
                    z *= 1 - hasAxis(currentAxisLock, AxisFlagBits::Z);

                    n = x + y + z;
                } else if (getNumActiveAxes(currentAxisLock) == 1) {
                    if (currentAxisLock == AxisFlagBits::X || currentAxisLock == AxisFlagBits::Z)
                        n = glm::vec3(0.0f, 1.0f, 0.0f);
                    else
                        n = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                float d = -glm::dot(originalObjectTransform.position, n);


                float t = -(glm::dot(cam.position, n) + d) / glm::dot(dir, n);

                glm::vec3 dif = (cam.position + dir * t) - originalObjectTransform.position;

                if (inputManager.keyHeld(SDL_SCANCODE_LCTRL) && !settings.objectSnapGlobal)
                    dif = glm::round(dif);

                selectedTransform.position = originalObjectTransform.position + filterAxes(dif, currentAxisLock);

                if (inputManager.keyHeld(SDL_SCANCODE_LCTRL) && settings.objectSnapGlobal)
                    selectedTransform.position = glm::round(selectedTransform.position);

                glm::mat4 vp = cam.getProjectionMatrix((float)windowSize.x / windowSize.y) * cam.getViewMatrix();

                if (getNumActiveAxes(currentAxisLock) == 2) {
                    ImGui::GetBackgroundDrawList()->AddLine(ImGui::GetMainViewport()->Pos + worldToScreen(glm::vec3(0.0f), vp), ImGui::GetMainViewport()->Pos + worldToScreen(n, vp), ImColor(1.0f, 1.0f, 1.0f), 2.0f);
                } else if (getNumActiveAxes(currentAxisLock) == 1) {
                    glm::vec3 x(1.0f, 0.0f, 0.0f);
                    glm::vec3 y(0.0f, 1.0f, 0.0f);
                    glm::vec3 z(0.0f, 0.0f, 1.0f);

                    x *= hasAxis(currentAxisLock, AxisFlagBits::X);
                    y *= hasAxis(currentAxisLock, AxisFlagBits::Y);
                    z *= hasAxis(currentAxisLock, AxisFlagBits::Z);

                    glm::vec3 start = selectedTransform.position - (x + y + z) * 5.0f;
                    glm::vec3 end = selectedTransform.position + (x + y + z) * 5.0f;

                    glm::vec2 startScreen = worldToScreenG(start, vp);
                    glm::vec2 endScreen = worldToScreenG(end, vp);

                    // clipping
                    //bool accept = lineClip(startScreen, endScreen, glm::vec2(0.0f), windowSize);

                    glm::vec3 color = x + y + z;

                    ImGui::GetBackgroundDrawList()->AddLine(ImGui::GetMainViewport()->Pos + glmToImgui(startScreen), ImGui::GetMainViewport()->Pos + glmToImgui(endScreen), ImColor(color.x, color.y, color.z), 2.0f);
                }


            } else if (currentTool == Tool::Rotate) {

            } else if (currentTool == Tool::Scale) {
                glm::vec2 halfWindowSize = glm::vec2(windowSize) * 0.5f;

                // Convert selected transform position from world space to screen space
                glm::vec4 ndcObjPosPreDivide = cam.getProjectionMatrix((float)windowSize.x / windowSize.y) * cam.getViewMatrix() * glm::vec4(selectedTransform.position, 1.0f);

                // NDC -> screen space
                glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
                ndcObjectPosition /= ndcObjPosPreDivide.w;
                ndcObjectPosition *= 0.5f;
                ndcObjectPosition += 0.5f;
                ndcObjectPosition *= windowSize;
                // Not sure why flipping Y is necessary?
                ndcObjectPosition.y = windowSize.y - ndcObjectPosition.y;

                glm::vec2 ndcMousePos = inputManager.getMousePosition();

                if (startingMouseDistance == -1.0f) {
                    startingMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);
                }

                float currentMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);

                // 1.0 scale circle
                glm::vec2 circlePos = ndcObjectPosition;
                ImGui::GetBackgroundDrawList()->AddCircle(ImGui::GetMainViewport()->Pos + ImVec2(circlePos.x, circlePos.y), startingMouseDistance, ImColor(1.0f, 1.0f, 1.0f), getCircleSegments(startingMouseDistance));

                // Line from mouse to scale circle
                glm::vec2 mouseDir = glm::normalize(ndcMousePos - circlePos);
                glm::vec2 lineStart = circlePos + (mouseDir * startingMouseDistance);
                ImGui::GetBackgroundDrawList()->AddLine(ImGui::GetMainViewport()->Pos + ImVec2(lineStart.x, lineStart.y), ImGui::GetMainViewport()->Pos + ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f));

                float scaleFac = (currentMouseDistance - startingMouseDistance) * 0.01f;

                // Probably don't want negative scale - scale < 1.0 is more useful
                if (scaleFac < 0.0f) {
                    scaleFac = (glm::pow(1.0f / (-scaleFac + 1.0f), 3.0f)) - 1.0f;
                }

                // Snap to increments of 0.1
                if (inputManager.keyHeld(SDL_SCANCODE_LCTRL)) {
                    scaleFac = glm::round(scaleFac / settings.scaleSnapIncrement) * settings.scaleSnapIncrement;
                }

                selectedTransform.scale = originalObjectTransform.scale + filterAxes(originalObjectTransform.scale * scaleFac, currentAxisLock);

                std::string scaleStr = "Avg. Scale: " + std::to_string(glm::dot(filterAxes(glm::vec3(1.0f), currentAxisLock), selectedTransform.scale) / getNumActiveAxes(currentAxisLock));

                ImGui::GetForegroundDrawList()->AddText(ImGui::GetMainViewport()->Pos + ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f), scaleStr.c_str());
            }
        }

        ImVec2 offset = ImGui::GetMainViewport()->Pos;

        updateCamera(deltaTime);

        if (reg.valid(currentSelectedEntity)) {
            auto& selectedTransform = reg.get<Transform>(currentSelectedEntity);
            // Convert selected transform position from world space to screen space
            glm::vec4 ndcObjPosPreDivide = cam.getProjectionMatrix((float)windowSize.x / windowSize.y) * cam.getViewMatrix() * glm::vec4(selectedTransform.position, 1.0f);

            // NDC -> screen space
            glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
            ndcObjectPosition /= ndcObjPosPreDivide.w;
            ndcObjectPosition *= 0.5f;
            ndcObjectPosition += 0.5f;
            ndcObjectPosition *= windowSize;
            // Not sure why flipping Y is necessary?
            ndcObjectPosition.y = windowSize.y - ndcObjectPosition.y;

            ImGui::GetBackgroundDrawList()->AddCircleFilled(glmToImgui(ndcObjectPosition) + offset, 7.0f, ImColor(1.0f, 0.25f, 1.0f));

            if (enableTransformGadget) {
                ImGuizmo::BeginFrame();
                ImGuizmo::Enable(true);
                ImGuizmo::SetRect(offset.x, offset.y, (float)windowSize.x, (float)windowSize.y);
                glm::mat4 view = cam.getViewMatrix();
                glm::mat4 proj = cam.getProjectionMatrix((float)windowSize.x / (float)windowSize.y);
                glm::mat4 tfMtx = selectedTransform.getMatrix();
                float snap = inputManager.keyHeld(SDL_SCANCODE_LCTRL) ? 15.0f : 0.0f;
                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD, glm::value_ptr(tfMtx), nullptr, &snap);

                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(tfMtx, scale, rotation, translation, skew, perspective);
                selectedTransform.position = translation;
                selectedTransform.rotation = rotation;
                selectedTransform.scale = scale;
            }

            if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT) &&
                inputManager.keyPressed(SDL_SCANCODE_D) &&
                !inputManager.mouseButtonHeld(MouseButton::Right)) {
                auto newEnt = reg.create();

                copyComponent<Transform>(currentSelectedEntity, newEnt, reg);
                copyComponent<WorldObject>(currentSelectedEntity, newEnt, reg);
                copyComponent<WorldLight>(currentSelectedEntity, newEnt, reg);

                for (auto& mdata : ComponentMetadataManager::metadata) {
                    ENTT_ID_TYPE t[] = { mdata.second.typeId };
                    auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));
                    if (!rtView.contains(currentSelectedEntity))
                        continue;

                    if (mdata.second.cloneFuncPtr)
                        mdata.second.cloneFuncPtr(currentSelectedEntity, newEnt, reg);
                }

                select(newEnt);
                activateTool(Tool::Translate);
            }

            if (ImGui::Begin("Selected entity")) {
                ImGui::Separator();

                for (auto& mdataPair : ComponentMetadataManager::metadata) {
                    auto& mdata = mdataPair.second;
                    ENTT_ID_TYPE t[] = { mdata.typeId };
                    auto rtView = reg.runtime_view(std::cbegin(t), std::cend(t));

                    if (rtView.contains(currentSelectedEntity) && mdata.editFuncPtr != nullptr) {
                        mdata.editFuncPtr(currentSelectedEntity, reg);
                    }
                }

                if (reg.has<WorldLight>(currentSelectedEntity)) {
                    ImGui::Text("WorldLight");
                    ImGui::SameLine();
                    if (ImGui::Button("Remove##WL")) {
                        reg.remove<WorldLight>(currentSelectedEntity);
                    } else {
                        auto& worldLight = reg.get<WorldLight>(currentSelectedEntity);
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
                        if (rtView.contains(currentSelectedEntity))
                            continue;

                        if (ImGui::Button(mdata.name.c_str())) {
                            mdata.addFuncPtr(currentSelectedEntity, reg);
                        }
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::End();

            if (inputManager.keyPressed(SDL_SCANCODE_DELETE)) {
                activateTool(Tool::None);
                reg.destroy(currentSelectedEntity);
            }
        }

        if (ImGui::Begin("Entity List")) {
            reg.each([this](auto ent) {
                ImGui::PushID((int)ent);
                ImGui::Text("Entity %u", ent);
                ImGui::SameLine();
                if (ImGui::Button("Select"))
                    select(ent);
                ImGui::PopID();
                });
        }
        ImGui::End();


        if (inputManager.keyPressed(SDL_SCANCODE_S) && inputManager.keyHeld(SDL_SCANCODE_LCTRL)) {
            ImGui::OpenPopup("Save Scene");
        }

        saveFileModal("Save Scene", [this](const char* path) {
            AssetID sceneId;
            std::string scenePath(path);
            if (g_assetDB.hasId(scenePath))
                sceneId = g_assetDB.getExistingID(scenePath);
            else
                sceneId = g_assetDB.createAsset(scenePath);
            saveScene(g_assetDB.createAsset(scenePath), reg);
            });


        if (inputManager.keyPressed(SDL_SCANCODE_O) && inputManager.keyHeld(SDL_SCANCODE_LCTRL)) {
            ImGui::OpenPopup("Open Scene");
        }

        openFileModal("Open Scene", [this](const char* path) {
            reg.clear();
            loadScene(g_assetDB.addOrGetExisting(path), reg);
            });

        if (inputManager.keyPressed(SDL_SCANCODE_I) && inputManager.keyHeld(SDL_SCANCODE_LCTRL) && inputManager.keyHeld(SDL_SCANCODE_I)) {
            imguiMetricsOpen = !imguiMetricsOpen;
        }

        if (imguiMetricsOpen)
            ImGui::ShowMetricsWindow(&imguiMetricsOpen);

        static std::vector<ShaderMetadata> shaderMdata;
        if (ImGui::Button("Generate shader metadata cache")) {
            shaderMdata.clear();
            PHYSFS_enumerate("Shaders", [](void*, const char* origDir, const char* fName) {
                std::string path = origDir;
                path += "/";
                path += fName;

                PHYSFS_File* f = PHYSFS_openRead(path.c_str());
                size_t len = PHYSFS_fileLength(f);
                void* data = std::malloc(len);

                PHYSFS_readBytes(f, data, len);
                PHYSFS_close(f);

                logMsg("%s:", fName);
                shaderMdata.push_back(generateSpirvMetadata((uint32_t*)data, len));

                std::free(data);

                return PHYSFS_ENUM_OK;
                }, nullptr);
        }

        for (ShaderMetadata& smData : shaderMdata) {
            if (!smData.valid) {
                ImGui::Text("%s (invalid)", smData.sourceFile.c_str());
            } else {
                ImGui::Text("%s:", smData.sourceFile.c_str());

                if (smData.lightBufferBinding.has_value())
                    ImGui::BulletText("Light buffer binding: %u", smData.lightBufferBinding);

                if (smData.modelMatrixBinding.has_value())
                    ImGui::BulletText("Model matrix binding: %u", smData.modelMatrixBinding);

                for (auto& outVar : smData.outputVars) {
                    ImGui::BulletText("Output variable: %s (location %u)", outVar.name.c_str(), outVar.location);
                }

                for (auto& inputVar : smData.inputVars) {
                    ImGui::BulletText("Input variable: %s (location %u)", inputVar.name.c_str(), inputVar.location);
                }
            }
        }

        if (ImGui::Button("Save AssetDB")) {
            g_assetDB.save();
        }

        if (ImGui::Button("Create Cube")) {
            AssetID cubeId = g_assetDB.addOrGetExisting("model.obj");

            createModelObject(reg, glm::vec3(0.0f), glm::quat(), cubeId, g_assetDB.addOrGetExisting("Materials/dev.json"), glm::vec3(1.0f));
        }

        struct EnumerateCallbackArgs {
            entt::registry& reg;
            std::string& currentDir;
        };

        static std::string currentDir = "";
        if (ImGui::Begin("Assets")) {
            EnumerateCallbackArgs enumCallbackArgs{
                reg,
                currentDir
            };

            if (ImGui::Button("..")) {
                std::filesystem::path p{ currentDir };
                currentDir = p.parent_path().string();
                if (currentDir == "/")
                    currentDir = "";

                if (currentDir[0] == '/') {
                    currentDir = currentDir.substr(1);
                }
                logMsg("Navigated to %s", currentDir.c_str());
            }

            PHYSFS_enumerate(currentDir.c_str(), [](void* regPtr, const char* origDir, const char* fName) {
                EnumerateCallbackArgs* callbackArgs = (EnumerateCallbackArgs*)regPtr;
                entt::registry& reg = callbackArgs->reg;

                std::string origDirStr = origDir;
                if (origDirStr[0] == '/') {
                    origDirStr = origDirStr.substr(1);
                }

                std::string fullPath;

                if (origDirStr.empty())
                    fullPath = fName;
                else
                    fullPath = origDirStr + "/" + std::string(fName);

                PHYSFS_Stat stat;
                PHYSFS_stat(fullPath.c_str(), &stat);

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    if (ImGui::Button(fName)) {
                        if (callbackArgs->currentDir != "/")
                            callbackArgs->currentDir += "/";
                        callbackArgs->currentDir += fName;

                        if (currentDir[0] == '/') {
                            currentDir = currentDir.substr(1);
                        }
                        logMsg("Navigated to %s", currentDir.c_str());
                        return PHYSFS_ENUM_STOP;
                    }
                } else {
                    AssetID id = g_assetDB.addOrGetExisting(fullPath);
                    if (g_assetDB.getAssetExtension(id) == ".obj" || g_assetDB.getAssetExtension(id) == ".mdl") {
                        if (ImGui::Button(fName)) {
                            entt::entity ent = createModelObject(reg, glm::vec3(), glm::quat(), id, g_assetDB.addOrGetExisting("Materials/dev.json"));

                            if (g_assetDB.getAssetExtension(id) == ".mdl") {
                                WorldObject& wo = reg.get<WorldObject>(ent);
                                setupSourceMaterials(id, wo);
                            }
                        }
                    } else {
                        ImGui::Text("%s (%s)", fName, g_assetDB.getAssetExtension(id).c_str());
                    }
                }
                return PHYSFS_ENUM_OK;
                }, &enumCallbackArgs);
        }

        ImGui::End();
    }
}