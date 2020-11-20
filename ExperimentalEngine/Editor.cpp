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
#include "NameComponent.hpp"
#include "Audio.hpp"
#include "EditorWindows.hpp"
#include "CreateModelObject.hpp"
#include "VKImGUIUtil.hpp"
#include "IconsFontAwesome5.h"
#include "IconsFontaudio.h"
#undef near
#undef far

namespace worlds {
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
        if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT u8" Transform")) {
            auto& selectedTransform = reg.get<Transform>(ent);
            ImGui::DragFloat3("Position", &selectedTransform.position.x);

            glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(selectedTransform.rotation));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot))) {
                selectedTransform.rotation = glm::radians(eulerRot);
            }

            ImGui::DragFloat3("Scale", &selectedTransform.scale.x);
            ImGui::Separator();
        }
    }

    void editWorldObject(entt::entity ent, entt::registry& reg) {
        if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" WorldObject")) {
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
            }

            ImGui::Separator();
        }
    }

    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" }
    };

    void createLight(entt::entity ent, entt::registry& reg) {
        reg.emplace<WorldLight>(ent);
    }

    void editLight(entt::entity ent, entt::registry& reg) {
        if (ImGui::CollapsingHeader(ICON_FA_LIGHTBULB u8" Light")) {
            if (ImGui::Button("Remove##WL")) {
                reg.remove<WorldLight>(ent);
            } else {
                auto& worldLight = reg.get<WorldLight>(ent);
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
    }

    void cloneLight(entt::entity from, entt::entity to, entt::registry& reg) {
        reg.emplace<WorldLight>(to, reg.get<WorldLight>(from));
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

        int i = 0;
        for (auto it = actor.physicsShapes.begin(); it != actor.physicsShapes.end(); it++) {
            ImGui::PushID(i);
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

            ImGui::DragFloat3("Position", &it->pos.x);

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
            ImGui::PopID();
            i++;
        }

        if (erase)
            actor.physicsShapes.erase(eraseIter);
    }

    void editPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<PhysicsActor>(ent);
        if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Actor")) {
            if (ImGui::Button("Remove##PA")) {
                reg.remove<PhysicsActor>(ent);
            } else {
                if (ImGui::Button("Update Collisions")) {
                    updatePhysicsShapes(pa);
                }

                editPhysicsShapes(pa);
            }

            ImGui::Separator();
        }
    }

    void editDynamicPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<DynamicPhysicsActor>(ent);
        if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Dynamic Physics Actor")) {
            if (ImGui::Button("Remove##DPA")) {
                reg.remove<DynamicPhysicsActor>(ent);
            } else {
                ImGui::DragFloat("Mass", &pa.mass);
                if (ImGui::Button("Update Collisions##DPA")) {
                    updatePhysicsShapes(pa);
                    physx::PxRigidBodyExt::updateMassAndInertia(*((physx::PxRigidDynamic*)pa.actor), &pa.mass, 1);
                }

                editPhysicsShapes(pa);
            }

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

    void editNameComponent(entt::entity ent, entt::registry& registry) {
        auto& nc = registry.get<NameComponent>(ent);
        
        ImGui::InputText("Name", &nc.name);
        ImGui::Separator();
    }

    void createNameComponent(entt::entity ent, entt::registry& registry) {
        registry.emplace<NameComponent>(ent);
    }

    void cloneNameComponent(entt::entity a, entt::entity b, entt::registry& reg) {
        reg.emplace<NameComponent>(b, reg.get<NameComponent>(a));
    }

    void editAudioSource(entt::entity ent, entt::registry& registry) {
        auto& as = registry.get<AudioSource>(ent);

        if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio Source")) {
            ImGui::Checkbox("Loop", &as.loop);
            ImGui::Checkbox("Spatialise", &as.spatialise);
            ImGui::Checkbox("Play on scene open", &as.playOnSceneOpen);
            ImGui::Text("Current Asset Path: %s", g_assetDB.getAssetPath(as.clipId).c_str());

            static std::string newPath;
            static bool canOpen = true;
            if (ImGui::BeginPopup("Audio Source Path")) {
                ImGui::InputText("Path", &newPath);

                if (ImGui::Button("Set")) {
                    if (!PHYSFS_exists(newPath.c_str())) {
                        canOpen = false;
                    } else {
                        as.clipId = g_assetDB.addOrGetExisting(newPath);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }

                if (!canOpen) {
                    ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f, 1.0f), "Couldn't open file.");
                }
                ImGui::EndPopup();
            }

            if (ImGui::Button("Change")) {
                newPath = g_assetDB.getAssetPath(as.clipId);
                canOpen = true;
                ImGui::OpenPopup("Audio Source Path");
            }

            if (ImGui::Button(ICON_FA_PLAY u8" Preview"))
                AudioSystem::getInstance()->playOneShotClip(as.clipId, glm::vec3(0.0f));

            ImGui::Separator();
        }
    }

    void createAudioSource(entt::entity ent, entt::registry& reg) {
        reg.emplace<AudioSource>(ent, g_assetDB.addOrGetExisting("Audio/SFX/dlgsound.ogg"));
    }

    void cloneAudioSource(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& asA = reg.get<AudioSource>(a);

        auto& asB = reg.emplace<AudioSource>(a, asA);
    }

    Editor::Editor(entt::registry& reg, EngineInterfaces interfaces)
        : reg(reg)
        , interfaces(interfaces)
        , cam(*interfaces.mainCamera)
        , currentTool(Tool::None)
        , currentAxisLock(AxisFlagBits::All)
        , lookX(0.0f)
        , lookY(0.0f)
        , settings()
        , imguiMetricsOpen(false)
        , currentSelectedEntity(entt::null)
        , enableTransformGadget(false)
        , startingMouseDistance(0.0f)
        , inputManager(*interfaces.inputManager)
        , active(true)
        , cameraSpeed(5.0f) {
        REGISTER_COMPONENT_TYPE(Transform, "Transform", true, editTransform, nullptr, nullptr);
        REGISTER_COMPONENT_TYPE(WorldObject, "WorldObject", true, editWorldObject, nullptr, nullptr);
        REGISTER_COMPONENT_TYPE(WorldLight, "WorldLight", true, editLight, createLight, cloneLight);
        REGISTER_COMPONENT_TYPE(PhysicsActor, "PhysicsActor", true, editPhysicsActor, createPhysicsActor, clonePhysicsActor);
        REGISTER_COMPONENT_TYPE(DynamicPhysicsActor, "DynamicPhysicsActor", true, editDynamicPhysicsActor, createDynamicPhysicsActor, cloneDynamicPhysicsActor);
        REGISTER_COMPONENT_TYPE(AudioSource, "AudioSource", true, editAudioSource, createAudioSource, cloneAudioSource);
        REGISTER_COMPONENT_TYPE(NameComponent, "NameComponent", true, editNameComponent, createNameComponent, cloneNameComponent);
        interfaces.engine->pauseSim = true;
        defaultMaterial = g_physics->createMaterial(0.5f, 0.5f, 0.1f);

        RTTPassCreateInfo sceneViewPassCI;
        sceneViewPassCI.enableShadows = true;
        sceneViewPassCI.width = 1600;
        sceneViewPassCI.height = 900;
        sceneViewPassCI.isVr = false;
        sceneViewPassCI.outputToScreen = false;
        sceneViewPassCI.useForPicking = true;
        sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);

        auto vkCtx = interfaces.renderer->getVKCtx();

        sceneViewDS = VKImGUIUtil::createDescriptorSetFor(interfaces.renderer->getSDRTarget(sceneViewPass), vkCtx);

#define ADD_EDITOR_WINDOW(type) editorWindows.push_back(std::make_unique<type>(interfaces, this))

        ADD_EDITOR_WINDOW(EntityList);
        ADD_EDITOR_WINDOW(Assets);
        ADD_EDITOR_WINDOW(EntityEditor);
        ADD_EDITOR_WINDOW(GameControls);
        ADD_EDITOR_WINDOW(StyleEditor);

#undef ADD_EDITOR_WINDOW
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
        float moveSpeed = cameraSpeed;

        static int origMouseX, origMouseY = 0;

        if (inputManager.mouseButtonPressed(MouseButton::Right, true)) {
            SDL_GetMouseState(&origMouseX, &origMouseY);
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
        currentAxisLock = AxisFlagBits::All;
        startingMouseDistance = -1.0f;
        logMsg(SDL_LOG_PRIORITY_DEBUG, "activateTool(%s)", toolStr(newTool));

        if (newTool != Tool::None)
            lastActiveTool = newTool;
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

    bool ctrlHeld(InputManager& inputManager) {
        return inputManager.keyHeld(SDL_SCANCODE_LCTRL, true) || inputManager.keyHeld(SDL_SCANCODE_RCTRL, true);
    }

    bool shiftHeld(InputManager& inputManager) {
        return inputManager.keyHeld(SDL_SCANCODE_LSHIFT, true) || inputManager.keyHeld(SDL_SCANCODE_RSHIFT, true);
    }

    void Editor::update(float deltaTime) {
        interfaces.renderer->setRTTPassActive(sceneViewPass, active);

        if (!active) {
            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && ctrlHeld(inputManager)) 
                g_console->executeCommandStr("reloadAndEdit");

            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && ctrlHeld(inputManager) && shiftHeld(inputManager))
                g_console->executeCommandStr("pauseAndEdit");
            return;
        }

        if (currentTool == Tool::None && reg.valid(currentSelectedEntity)) {
            // Right mouse button means that the view's being moved, so we'll ignore any tools
            // and assume the user's trying to move the camera
            if (!inputManager.mouseButtonHeld(MouseButton::Right, true)) {
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

            if (shiftHeld(inputManager)) {
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

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                for (auto& window : editorWindows) {
                    if (ImGui::MenuItem(window->getName())) {
                        window->setActive(!window->isActive());
                    }
                }
                ImGui::EndMenu();
            }
            

            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin(ICON_FA_EDIT u8" Editor")) {
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
            ImGui::Checkbox("Pause physics", &interfaces.engine->pauseSim);
            ImGui::InputFloat("Scale snap increment", &settings.scaleSnapIncrement, 0.1f, 0.5f);
        }
        ImGui::End();

        updateCamera(deltaTime);

        static ImVec2 currentSceneViewSize = ImVec2(0.0f, 0.0f);

        static ConVar noScenePad("editor_disableScenePad", "0");

        if (noScenePad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin(ICON_FA_MAP u8" Scene")) {
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();

            if (contentRegion.x != currentSceneViewSize.x || contentRegion.y != currentSceneViewSize.y) {
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
                sceneViewPass = interfaces.renderer->createRTTPass(sceneViewPassCI);
                vkCtx.device.freeDescriptorSets(vkCtx.descriptorPool, vk::DescriptorSet{ sceneViewDS });

                sceneViewDS = VKImGUIUtil::createDescriptorSetFor(interfaces.renderer->getSDRTarget(sceneViewPass), vkCtx);
            }
            auto wSize = ImGui::GetContentRegionAvail();

            ImGui::Image((ImTextureID)sceneViewDS, ImVec2(currentSceneViewSize.x, currentSceneViewSize.y));

            auto wPos = ImGui::GetWindowPos() + ImGui::GetCursorStartPos();
            auto mPos = ImGui::GetIO().MousePos;
            auto localMPos = mPos - wPos;

            if (currentTool != Tool::None) {
                auto& selectedTransform = reg.get<Transform>(currentSelectedEntity);

                if (inputManager.mouseButtonPressed(MouseButton::Right, true)) {
                    selectedTransform = originalObjectTransform;
                    currentTool = Tool::None;
                } else if (inputManager.mouseButtonPressed(MouseButton::Left, true)) {
                    currentTool = Tool::None;
                }

                if (currentTool == Tool::Translate) {
                    glm::vec2 halfWindowSize = convVec(wSize) * 0.5f;

                    glm::vec2 ndcMousePos = convVec(localMPos);
                    ndcMousePos -= halfWindowSize;
                    ndcMousePos.x /= halfWindowSize.x;
                    ndcMousePos.y /= halfWindowSize.y;
                    ndcMousePos *= -1.0f;

                    float aspect = wSize.x / wSize.y;
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

                    if (ctrlHeld(inputManager) && !settings.objectSnapGlobal)
                        dif = glm::round(dif);

                    selectedTransform.position = originalObjectTransform.position + filterAxes(dif, currentAxisLock);

                    if (ctrlHeld(inputManager) && settings.objectSnapGlobal)
                        selectedTransform.position = glm::round(selectedTransform.position);

                    glm::mat4 vp = cam.getProjectionMatrix((float)wSize.x / wSize.y) * cam.getViewMatrix();

                    if (getNumActiveAxes(currentAxisLock) == 2) {
                        ImGui::GetForegroundDrawList()->AddLine(wPos + worldToScreen(glm::vec3(0.0f), vp), wPos + worldToScreen(n, vp), ImColor(1.0f, 1.0f, 1.0f), 2.0f);
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

                        ImGui::GetForegroundDrawList()->AddLine(wPos + convVec(startScreen), wPos + convVec(endScreen), ImColor(color.x, color.y, color.z), 2.0f);
                    }


                } else if (currentTool == Tool::Rotate) {

                } else if (currentTool == Tool::Scale) {
                    glm::vec2 halfWindowSize = convVec(wSize) * 0.5f;

                    // Convert selected transform position from world space to screen space
                    glm::vec4 ndcObjPosPreDivide = cam.getProjectionMatrix(wSize.x / wSize.y) * cam.getViewMatrix() * glm::vec4(selectedTransform.position, 1.0f);

                    // NDC -> screen space
                    glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
                    ndcObjectPosition /= ndcObjPosPreDivide.w;
                    ndcObjectPosition *= 0.5f;
                    ndcObjectPosition += 0.5f;
                    ndcObjectPosition *= convVec(wSize);
                    // Not sure why flipping Y is necessary?
                    ndcObjectPosition.y = wSize.y - ndcObjectPosition.y;

                    glm::vec2 ndcMousePos = convVec(ImGui::GetIO().MousePos - wPos);

                    if (startingMouseDistance == -1.0f) {
                        startingMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);
                    }

                    float currentMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);

                    // 1.0 scale circle
                    glm::vec2 circlePos = ndcObjectPosition;
                    ImGui::GetWindowDrawList()->AddCircle(wPos + ImVec2(circlePos.x, circlePos.y), startingMouseDistance, ImColor(1.0f, 1.0f, 1.0f), getCircleSegments(startingMouseDistance));

                    // Line from mouse to scale circle
                    glm::vec2 mouseDir = glm::normalize(ndcMousePos - circlePos);
                    glm::vec2 lineStart = circlePos + (mouseDir * startingMouseDistance);
                    ImGui::GetWindowDrawList()->AddLine(wPos + ImVec2(lineStart.x, lineStart.y), wPos + ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f));

                    float scaleFac = (currentMouseDistance - startingMouseDistance) * 0.01f;

                    // Probably don't want negative scale - scale < 1.0 is more useful
                    if (scaleFac < 0.0f) {
                        scaleFac = (glm::pow(1.0f / (-scaleFac + 1.0f), 3.0f)) - 1.0f;
                    }

                    // Snap to increments of 0.1
                    if (ctrlHeld(inputManager)) {
                        scaleFac = glm::round(scaleFac / settings.scaleSnapIncrement) * settings.scaleSnapIncrement;
                    }

                    selectedTransform.scale = originalObjectTransform.scale + filterAxes(originalObjectTransform.scale * scaleFac, currentAxisLock);

                    std::string scaleStr = "Avg. Scale: " + std::to_string(glm::dot(filterAxes(glm::vec3(1.0f), currentAxisLock), selectedTransform.scale) / getNumActiveAxes(currentAxisLock));

                    ImGui::GetWindowDrawList()->AddText(wPos + ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f), scaleStr.c_str());
                }
            }

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

                if (enableTransformGadget) {
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
                        switch (lastActiveTool) {
                        case Tool::Scale:
                            snap = glm::vec3{ settings.scaleSnapIncrement };
                            break;
                        case Tool::Translate:
                            snap = glm::vec3{ 1.0f };
                            break;
                        case Tool::Rotate:
                            snap = glm::vec3{ 15.0f };
                            break;
                        }
                    }

                    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), toolToOp(lastActiveTool), ImGuizmo::MODE::WORLD, glm::value_ptr(tfMtx), nullptr, glm::value_ptr(snap));
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

                if (shiftHeld(inputManager) &&
                    inputManager.keyPressed(SDL_SCANCODE_D) &&
                    !inputManager.mouseButtonHeld(MouseButton::Right, true)) {
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

                if (inputManager.keyPressed(SDL_SCANCODE_DELETE)) {
                    activateTool(Tool::None);
                    reg.destroy(currentSelectedEntity);
                }
            }

            if (ImGui::IsWindowHovered() && !ImGuizmo::IsOver()) {
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

        if (inputManager.keyPressed(SDL_SCANCODE_S) && ctrlHeld(inputManager)) {
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


        if (inputManager.keyPressed(SDL_SCANCODE_O) && ctrlHeld(inputManager)) {
            ImGui::OpenPopup("Open Scene");
        }

        openFileModal("Open Scene", [this](const char* path) {
            reg.clear();
            loadScene(g_assetDB.addOrGetExisting(path), reg);
            });

        if (inputManager.keyPressed(SDL_SCANCODE_I, true) && 
            ctrlHeld(inputManager) && 
            shiftHeld(inputManager)) {
            imguiMetricsOpen = !imguiMetricsOpen;
        }

        if (inputManager.keyPressed(SDL_SCANCODE_P, true) && ctrlHeld(inputManager)) {
            interfaces.engine->pauseSim = false;
            g_console->executeCommandStr("play");
        }

        if (inputManager.keyPressed(SDL_SCANCODE_P, true) && ctrlHeld(inputManager) && shiftHeld(inputManager)) {
            g_console->executeCommandStr("unpause");
        }

        if (imguiMetricsOpen)
            ImGui::ShowMetricsWindow(&imguiMetricsOpen);
    }
}