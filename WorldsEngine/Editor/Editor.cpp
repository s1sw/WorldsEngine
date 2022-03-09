#include "Editor.hpp"
#include <Core/Engine.hpp>
#include <Core/Transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "GuiUtil.hpp"
#include <ComponentMeta/ComponentMetadata.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui_internal.h>
#include <Physics/PhysicsActor.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Physics/Physics.hpp>
#include <Core/Log.hpp>
#include <ImGui/imgui.h>
#include <ImGui/ImGuizmo.h>
#include <filesystem>
#include <Render/Loaders/SourceModelLoader.hpp>
#include <Core/NameComponent.hpp>
#include "EditorWindows/EditorWindows.hpp"
#include <Util/CreateModelObject.hpp>
#include <Util/VKImGUIUtil.hpp>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <ComponentMeta/ComponentFuncs.hpp>
#include <Physics/D6Joint.hpp>
#include <nlohmann/json.hpp>
#include <Audio/Audio.hpp>
#include <AssetCompilation/AssetCompilers.hpp>
#include "AssetEditors.hpp"
#include <Scripting/NetVM.hpp>
#include "ImGui/imgui_internal.h"
#include "tracy/Tracy.hpp"
#include <fstream>
#include <Libs/pcg_basic.h>
#include <Core/Window.hpp>

namespace worlds {
    std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> ComponentMetadataManager::metadata;
    std::vector<ComponentEditor*> ComponentMetadataManager::sorted;
    std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> ComponentMetadataManager::bySerializedID;
    std::unordered_map<std::string, ComponentEditor*> ComponentMetadataManager::byName;

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
        if (!project)
#ifndef NDEBUG
            return "Worlds Engine (debug)";
#else
            return "Worlds Engine";
#endif

        std::string base =
#ifndef NDEBUG
            "Worlds Engine Editor (debug) - "
#else
            "Worlds Engine Editor - "
#endif
            + std::string(project->name()) + " | "
            + reg.ctx<SceneInfo>().name;

        if (lastSaveModificationCount != undo.modificationCount()) {
            base += "*";
        }

        return base;
    }

    void Editor::updateWindowTitle() {
        static std::string lastTitle;
        std::string newTitle = generateWindowTitle();

        if (lastTitle != newTitle) {
            interfaces.engine->getMainWindow().setTitle(newTitle.c_str());
        }
    }

    static int menuButtonsExtent = 0;

    SDL_HitTestResult hitTest(SDL_Window* win, const SDL_Point* p, void* v) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);

        uint32_t windowFlags = SDL_GetWindowFlags(win);

        if (windowFlags & SDL_WINDOW_MAXIMIZED) {
            return SDL_HITTEST_NORMAL;
        }

        if (p->x > menuButtonsExtent && p->x < w - 135 && p->y < 20 && p->y > 0) {
            return SDL_HITTEST_DRAGGABLE;
        }


        enum BorderFlags {
            None = 0,
            Left = 1,
            Right = 2,
            Top = 4,
            Bottom = 8
        };

        const int RESIZE_BORDER = 5;
        int flags = None;

        if (p->x < RESIZE_BORDER && p->x > -RESIZE_BORDER) {
            flags |= Left;
        } else if (p->x > w - RESIZE_BORDER && p->x < w + RESIZE_BORDER) {
            flags |= Right;
        }

        if (p->y < RESIZE_BORDER && p->y > -RESIZE_BORDER) {
            flags |= Top;
        } else if (p->y > h - RESIZE_BORDER && p->y < h + RESIZE_BORDER) {
            flags |= Bottom;
        }

        switch (flags) {
        case Left: return SDL_HITTEST_RESIZE_LEFT;
        case Right: return SDL_HITTEST_RESIZE_RIGHT;
        case Top: return SDL_HITTEST_RESIZE_TOP;
        case Bottom: return SDL_HITTEST_RESIZE_BOTTOM;
        case Top | Left: return SDL_HITTEST_RESIZE_TOPLEFT;
        case Top | Right: return SDL_HITTEST_RESIZE_TOPRIGHT;
        case Bottom | Left: return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        case Bottom | Right: return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        }

        return SDL_HITTEST_NORMAL;
    }

    EntityFolder::EntityFolder(std::string name) : name(name) {
        randomId = pcg32_random();
    }

    Editor::Editor(entt::registry& reg, EngineInterfaces interfaces)
        : active(true)
        , currentSelectedAsset(~0u)
        , currentTool(Tool::Translate)
        , reg(reg)
        , currentSelectedEntity(entt::null)
        , lookX(0.0f)
        , lookY(0.0f)
        , cameraSpeed(5.0f)
        , imguiMetricsOpen(false)
        , settings()
        , interfaces(interfaces)
        , inputManager(*interfaces.inputManager) {
        ComponentMetadataManager::setupLookup();
        interfaces.engine->pauseSim = true;

#define ADD_EDITOR_WINDOW(type) editorWindows.add(std::make_unique<type>(interfaces, this))

        ADD_EDITOR_WINDOW(EntityList);
        ADD_EDITOR_WINDOW(Assets);
        ADD_EDITOR_WINDOW(GameControls);
        ADD_EDITOR_WINDOW(StyleEditor);
        ADD_EDITOR_WINDOW(MaterialEditor);
        ADD_EDITOR_WINDOW(AboutWindow);
        ADD_EDITOR_WINDOW(BakingWindow);
        ADD_EDITOR_WINDOW(SceneSettingsWindow);
        ADD_EDITOR_WINDOW(AssetEditor);
        ADD_EDITOR_WINDOW(RawAssets);
        ADD_EDITOR_WINDOW(AssetCompilationManager);

#undef ADD_EDITOR_WINDOW
        AssetCompilers::initialise();
        AssetEditors::initialise();
        SDL_Window* window = interfaces.engine->getMainWindow().getWrappedHandle();
        SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "1");
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowHitTest(window, hitTest, nullptr);
        sceneViews.add(new EditorSceneView{ interfaces, this });

        titleBarIcon = interfaces.renderer->uiTextureManager().loadOrGet(AssetDB::pathToId("UI/Editor/Images/logo_no_background.png"));

        EntityFolders folders;

        reg.set<EntityFolders>(std::move(folders));
        loadOpenWindows();
    }

#undef REGISTER_COMPONENT_TYPE

    Editor::~Editor() {
        saveOpenWindows();
    }

    void Editor::select(entt::entity entity) {
        // Remove selection from existing entity
        if (reg.valid(currentSelectedEntity) && reg.has<EditorGlow>(currentSelectedEntity)) {
            reg.remove<EditorGlow>(currentSelectedEntity);
        }

        currentSelectedEntity = entity;
        // A null entity means we should deselect the current entity
        if (!reg.valid(entity)) {
            for (auto ent : selectedEntities) {
                reg.remove_if_exists<EditorGlow>(ent);
            }
            selectedEntities.clear();
            return;
        }

        reg.emplace<EditorGlow>(currentSelectedEntity);
    }

    void Editor::multiSelect(entt::entity entity) {
        if (!reg.valid(entity)) return;

        if (!reg.valid(currentSelectedEntity)) {
            select(entity);
            return;
        }

        if (entity == currentSelectedEntity) {
            if (selectedEntities.numElements() == 0) {
                select(entt::null);
            } else {
                select(selectedEntities[0]);
                selectedEntities.removeAt(0);
            }
            return;
        }

        if (selectedEntities.contains(entity)) {
            selectedEntities.removeValue(entity);
            reg.remove_if_exists<EditorGlow>(entity);
        } else {
            reg.emplace<EditorGlow>(entity);
            selectedEntities.add(entity);
        }
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

    void Editor::activateTool(Tool newTool) {
        assert(reg.valid(currentSelectedEntity));
        currentTool = newTool;
        originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
    }

    bool Editor::isEntitySelected(entt::entity ent) const {
        if (currentSelectedEntity == ent) return true;

        for (entt::entity selectedEnt : selectedEntities) {
            if (selectedEnt == ent) return true;
        }

        return false;
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

    void Editor::overrideHandle(Transform* t) {
        handleOverriden = true;
        overrideTransform = t;
    }

    void Editor::overrideHandle(entt::entity e) {
        handleOverrideEntity = e;
    }

    bool Editor::entityEyedropper(entt::entity& picked) {
        entityEyedropperActive = true;

        if (reg.valid(eyedroppedEntity)) {
            picked = eyedroppedEntity;
            eyedroppedEntity = entt::null;
            entityEyedropperActive = false;
            return true;
        }

        return false;
    }

    void Editor::saveOpenWindows() {
        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::string openWindowPath = prefPath + std::string("openWindows.json");

        nlohmann::json j;

        for (auto& window : editorWindows) {
            j[window->getName()] = {
                { "open", window->isActive() }
            };
        }

        std::ofstream o{ openWindowPath };
        o << std::setw(2) << j;
        o.close();
    }

    void Editor::loadOpenWindows() {
        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::string openWindowPath = prefPath + std::string("openWindows.json");

        nlohmann::json j;
        std::ifstream i{ openWindowPath };

        if (!i.good()) return;

        i >> j;
        i.close();

        for (auto& p : j.items()) {
            std::string name = p.key();
            for (auto& win : editorWindows) {
                if (win->getName() == name) {
                    win->setActive(p.value().value("open", win->isActive()));
                }
            }
        }
    }

    EditorSceneView* Editor::getFirstSceneView() {
        return sceneViews[0];
    }

    void Editor::eyedropperSelect(entt::entity picked) {
        eyedroppedEntity = picked;
    }

    void Editor::handleTools(Transform& t, ImVec2 wPos, ImVec2 wSize, Camera& camera) {
        // Convert selected transform position from world space to screen space
        glm::vec4 ndcObjPosPreDivide = camera.getProjectionMatrix((float)wSize.x / wSize.y) * camera.getViewMatrix() * glm::vec4(t.position, 1.0f);

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

        glm::mat4 view = camera.getViewMatrix();
        // Get a relatively normal projection matrix so ImGuizmo doesn't break.
        glm::mat4 proj = camera.getProjectionMatrixZONonInfinite((float)wSize.x / (float)wSize.y);

        glm::mat4 tfMtx = t.getMatrix();
        glm::vec3 snap{ 0.0f };

        static glm::vec3 boundsScale {1.0f};

        if (inputManager.keyHeld(SDL_SCANCODE_LCTRL, true)) {
            switch (currentTool) {
            case Tool::Rotate:
                snap = glm::vec3{ settings.angularSnapIncrement };
                break;
            case Tool::Bounds:
                snap = boundsScale;//glm::vec3{ settings.snapIncrement } / t.scale;
                break;
            default:
                snap = glm::vec3{ settings.snapIncrement };
                break;
            }
        }

        static float bounds[6]{ -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

        glm::mat4 deltaMatrix{ 1.0f };

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
            toolToOp(currentTool), toolLocalSpace ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD,
            glm::value_ptr(tfMtx), glm::value_ptr(deltaMatrix), glm::value_ptr(snap),
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
            boundsScale = glm::vec3{ settings.snapIncrement } / t.scale;
        }

        usingLast = ImGuizmo::IsUsing();

        switch (currentTool) {
        case Tool::Translate:
            t.position = translation;
            break;
        case Tool::Rotate:
            t.rotation = rotation;
            break;
        case Tool::Scale:
            t.scale = scale;
            break;
        case Tool::Bounds:
            t.position = translation;
            t.rotation = rotation;
            t.scale = scale;
            break;
        default:
            break;
        }

        if (!handleOverriden) {
            for (auto ent : selectedEntities) {
                auto& msTransform = reg.get<Transform>(ent);
                msTransform.fromMatrix(deltaMatrix * msTransform.getMatrix());
            }
        }
    }

    static ConVar integratedMenuBar{ "ed_integratedMenuBar", "1" };

    void Editor::drawMenuBarTitle() {
        const char* windowTitle = interfaces.engine->getMainWindow().getTitle();
        ImVec2 textSize = ImGui::CalcTextSize(windowTitle);
        ImVec2 menuBarCenter = ImGui::GetWindowSize() * 0.5f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (integratedMenuBar.getInt()) {
            drawList->AddText(ImVec2(menuBarCenter.x - (textSize.x * 0.5f), ImGui::GetWindowHeight() * 0.15f), ImColor(255, 255, 255), windowTitle);

            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_FALSE);
            float barWidth = ImGui::GetWindowWidth();
            float barHeight = ImGui::GetWindowHeight();
            const float crossSize = 6.0f;
            ImVec2 crossCenter(ImGui::GetWindowWidth() - 17.0f - crossSize, menuBarCenter.y);
            crossCenter -= ImVec2(0.5f, 0.5f);
            auto crossColor = ImColor(255, 255, 255);

            ImVec2 mousePos = ImGui::GetMousePos();

            if (mousePos.x > barWidth - 45.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    interfaces.engine->quit();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 45.0f, 0.0f), ImVec2(barWidth, barHeight), ImColor(255, 0, 0, 255));
            }

            drawList->AddLine(crossCenter + ImVec2(+crossSize, +crossSize), crossCenter + ImVec2(-crossSize, -crossSize), crossColor, 1.0f);
            drawList->AddLine(crossCenter + ImVec2(+crossSize, -crossSize), crossCenter + ImVec2(-crossSize, +crossSize), crossColor, 1.0f);

            Window& window = interfaces.engine->getMainWindow();
            ImVec2 maximiseCenter(barWidth - 45.0f - 22.0f, menuBarCenter.y);
            if (mousePos.x > barWidth - 90.0f && mousePos.x < barWidth - 45.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (window.isMaximised())
                        window.restore();
                    else
                        window.maximise();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 45.0f - 45.0f, 0.0f), ImVec2(barWidth - 45.0f, barHeight), ImColor(255, 255, 255, 50));
            }

            if (!window.isMaximised()) {
                drawList->AddRect(maximiseCenter - ImVec2(crossSize, crossSize), maximiseCenter + ImVec2(crossSize, crossSize), ImColor(255, 255, 255));
            } else {
                drawList->AddRect(maximiseCenter - ImVec2(crossSize - 3, crossSize), maximiseCenter + ImVec2(crossSize, crossSize - 3), ImColor(255, 255, 255));
                drawList->AddRectFilled(maximiseCenter - ImVec2(crossSize, crossSize - 3), maximiseCenter + ImVec2(crossSize - 3, crossSize), ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                drawList->AddRect(maximiseCenter - ImVec2(crossSize, crossSize - 3), maximiseCenter + ImVec2(crossSize - 3, crossSize), ImColor(255, 255, 255));
            }

            ImVec2 minimiseCenter(barWidth - 90.0f - 22.0f, menuBarCenter.y);
            if (mousePos.x > barWidth - 135.0f && mousePos.x < barWidth - 90.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {

                    if (window.isMinimised())
                        window.restore();
                    else
                        window.minimise();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 135.0f, 0.0f), ImVec2(barWidth - 90.0f, barHeight), ImColor(255, 255, 255, 50));
            }
            drawList->AddRectFilled(minimiseCenter - ImVec2(5, 0), minimiseCenter + ImVec2(5, 1), ImColor(255, 255, 255));
        } else {
            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_TRUE);
        }
    }

    void Editor::openProject(std::string path) {
        ZoneScoped;

        project = std::make_unique<GameProject>(path);
        project->mountPaths();
        interfaces.renderer->reloadContent(worlds::ReloadFlags::All);

        // Update recent projects list
        std::vector<std::string> recentProjects;

        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::ifstream recentProjectsStream(prefPath + std::string{ "recentProjects.txt" });

        if (recentProjectsStream.good()) {
            std::string currLine;

            while (std::getline(recentProjectsStream, currLine)) {
                recentProjects.push_back(currLine);
            }
        }

        recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), path), recentProjects.end());
        recentProjects.insert(recentProjects.begin(), path);

        std::ofstream recentProjectsOutStream(prefPath + std::string{ "recentProjects.txt" });

        if (recentProjectsOutStream.good()) {
            for (std::string& path : recentProjects) {
                recentProjectsOutStream << path << "\n";
            }
        }

        SDL_free(prefPath);

        AudioSystem::getInstance()->loadMasterBanks();
    }

    void Editor::update(float deltaTime) {
        ZoneScoped;

        if (!active) {
            drawPopupNotifications();
            for (EditorSceneView* esv : sceneViews) {
                esv->setViewportActive(false);
            }

            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && inputManager.ctrlHeld() && !inputManager.shiftHeld())
                g_console->executeCommandStr("reloadAndEdit");

            if (inputManager.keyPressed(SDL_SCANCODE_P, true) && inputManager.ctrlHeld() && inputManager.shiftHeld())
                g_console->executeCommandStr("pauseAndEdit");

            if (ImGui::BeginMainMenuBar()) {
                ImGui::Image(titleBarIcon, ImVec2(24, 24));
                if (ImGui::MenuItem("Stop Playing")) {
                    g_console->executeCommandStr("reloadAndEdit");
                }

                if (ImGui::MenuItem("Pause and Edit")) {
                    g_console->executeCommandStr("pauseAndEdit");
                }

                menuButtonsExtent = ImGui::GetCursorPosX();

                drawMenuBarTitle();
                ImGui::EndMainMenuBar();
            }
            return;
        }

        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), [](EditorSceneView* esv) {
            if (!esv->open) {
                delete esv;
                return true;
            }
            return false;
            }), sceneViews.end());

        if (!interfaces.engine->getMainWindow().isFocused()) {
            for (EditorSceneView* esv : sceneViews) {
                esv->setViewportActive(false);
            }
        } else {
            for (EditorSceneView* esv : sceneViews) {
                esv->setViewportActive(true);
            }
        }

        AudioSystem::getInstance()->stopEverything(reg);

        if (!project) {
            static std::vector<std::string> recentProjects;
            static bool loadedRecentProjects = false;

            if (!loadedRecentProjects) {
                loadedRecentProjects = true;
                char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
                std::ifstream recentProjectsStream(prefPath + std::string{ "recentProjects.txt" });

                if (recentProjectsStream.good()) {
                    std::string currLine;

                    while (std::getline(recentProjectsStream, currLine)) {
                        recentProjects.push_back(currLine);
                    }
                }

                SDL_free(prefPath);
            }

            updateWindowTitle();

            ImVec2 menuBarSize;
            if (ImGui::BeginMainMenuBar()) {
                if (integratedMenuBar.getInt())
                    ImGui::Image(titleBarIcon, ImVec2(24, 24));
                menuButtonsExtent = 24;

                menuBarSize = ImGui::GetWindowSize();
                drawMenuBarTitle();
                ImGui::EndMainMenuBar();
            }

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos + ImVec2(0.0f, menuBarSize.y));
            ImGui::SetNextWindowSize(viewport->Size - ImVec2(0.0f, menuBarSize.y));
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("ProjectWindow", 0,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::PopStyleVar(2);

            ImGui::Text("Select a project.");

            openFileFullFSModal("Open Project", [&](const char* path) {
                openProject(path);
                });

            if (ImGui::Button("Open")) {
                ImGui::OpenPopup("Open Project");
            }

            for (std::string& path : recentProjects) {
                if (ImGui::Button(path.c_str())) {
                    openProject(path);
                }
            }

            ImGui::End();

            return;
        }

        handleOverriden = false;

        updateWindowTitle();

        // Create global dock space
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorDockspaceWindow", 0,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleVar(3);
        ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0, 0));
        ImGui::End();

        // Draw black background
        ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos, viewport->Size, ImColor(0.0f, 0.0f, 0.0f, 1.0f));

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

        if (ImGui::Begin(ICON_FA_EDIT u8" Editor")) {
            ImGui::Text("Current tool: %s", toolStr(currentTool));

            ImGui::Checkbox("Manipulate in local space", &toolLocalSpace);

            ImGui::Checkbox("Global object snap", &settings.objectSnapGlobal);
            tooltipHover("If this is checked, moving an object with Ctrl held will snap in increments relative to the world rather than the object's original position.");
            ImGui::Checkbox("Pause physics", &interfaces.engine->pauseSim);
            ImGui::InputFloat("Snap increment", &settings.snapIncrement, 0.1f, 0.5f);
            ImGui::InputFloat("Angular snap increment", &settings.angularSnapIncrement, 0.5f, 1.0f);
            ImGui::InputFloat("Camera speed", &cameraSpeed, 0.1f);

            ImGui::InputFloat("Scene icon distance", &settings.sceneIconDrawDistance);
            if (ImGui::Checkbox("Shadows", &settings.enableShadows)) {
                for (EditorSceneView* esv : sceneViews) {
                    esv->setShadowsEnabled(settings.enableShadows);
                }
            }
        }
        ImGui::End();

        for (auto& edWindow : editorWindows) {
            if (edWindow->isActive()) {
                edWindow->draw(reg);
            }
        }

        static ConVar ed_saveAsJson { "ed_saveAsJson", "0", "Save scene files as JSON rather than MessagePack." };
        if (inputManager.keyPressed(SDL_SCANCODE_S) && inputManager.ctrlHeld()) {
            if (reg.ctx<SceneInfo>().id != ~0u && !inputManager.shiftHeld()) {
                AssetID sceneId = reg.ctx<SceneInfo>().id;
                if (ed_saveAsJson.getInt())
                    JsonSceneSerializer::saveScene(sceneId, reg);
                else
                    MessagePackSceneSerializer::saveScene(sceneId, reg);

                lastSaveModificationCount = undo.modificationCount();
            } else {
                ImGui::OpenPopup("Save Scene");
            }
        }


        if (inputManager.keyPressed(SDL_SCANCODE_N) && inputManager.ctrlHeld()) {
            messageBoxModal("New Scene",
                "Are you sure you want to clear the current scene and create a new one?",
                [&](bool result) {
                    if (result) {
                        interfaces.engine->createStartupScene();
                        updateWindowTitle();
                    }
                });
        }

        if (inputManager.keyPressed(SDL_SCANCODE_C) && inputManager.ctrlHeld() && reg.valid(currentSelectedEntity)) {
            std::string entityJson = JsonSceneSerializer::entityToJson(reg, currentSelectedEntity);
            SDL_SetClipboardText(entityJson.c_str());
        }

        if (inputManager.keyPressed(SDL_SCANCODE_V) && inputManager.ctrlHeld() && SDL_HasClipboardText()) {
            const char* txt = SDL_GetClipboardText();
            try {
                select(JsonSceneSerializer::jsonToEntity(reg, txt));
                addNotification("Entity pasted! :)");
            }             catch (nlohmann::detail::exception& e) {
                logErr("Failed to deserialize clipboard entity: %s", e.what());
                addNotification("Sorry, we couldn't paste that into the scene.", NotificationType::Error);
            }
        }

        saveFileModal("Save Scene", [this](const char* path) {
            AssetID sceneId = AssetDB::createAsset(path);
            JsonSceneSerializer::saveScene(sceneId, reg);
            lastSaveModificationCount = undo.modificationCount();
            interfaces.engine->loadScene(sceneId);
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
            interfaces.engine->loadScene(AssetDB::pathToId(path));
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

        std::string popupToOpen;

        if (ImGui::BeginMainMenuBar()) {
            if (integratedMenuBar.getInt())
                ImGui::Image(titleBarIcon, ImVec2(24, 24));
            if (ImGui::BeginMenu("File")) {
                for (auto& window : editorWindows) {
                    if (window->menuSection() == EditorMenu::File) {
                        if (ImGui::MenuItem(window->getName())) {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                if (ImGui::MenuItem("New")) {
                    popupToOpen = "New Scene";
                }

                if (ImGui::MenuItem("Open")) {
                    popupToOpen = "Open Scene";
                }

                if (ImGui::MenuItem("Save")) {
                    if (reg.ctx<SceneInfo>().id != ~0u && !inputManager.shiftHeld()) {
                        AssetID sceneId = reg.ctx<SceneInfo>().id;
                        JsonSceneSerializer::saveScene(sceneId, reg);
                        lastSaveModificationCount = undo.modificationCount();
                    } else {
                        popupToOpen = "Save Scene";
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Close Project")) {
                    project->unmountPaths();
                    project.reset();
                    interfaces.renderer->reloadContent(worlds::ReloadFlags::All);
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

                ImGui::Separator();


                if (ImGui::MenuItem("Create Prefab", nullptr, false, reg.valid(currentSelectedEntity))) {
                    popupToOpen = "Save Prefab";
                }

                if (ImGui::MenuItem("New Scene View")) {
                    sceneViews.add(new EditorSceneView{ interfaces, this });
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

            menuButtonsExtent = ImGui::GetCursorPosX();

            drawMenuBarTitle();

            ImGui::EndMainMenuBar();
        }

        saveFileModal("Save Prefab", [&](const char* path) {
            PHYSFS_File* file = PHYSFS_openWrite(path);
            JsonSceneSerializer::saveEntity(file, reg, currentSelectedEntity);
            });

        interfaces.scriptEngine->onEditorUpdate(deltaTime);

        int sceneViewIndex = 0;
        for (EditorSceneView* sceneView : sceneViews) {
            sceneView->drawWindow(sceneViewIndex++);
        }

        if (!popupToOpen.empty())
            ImGui::OpenPopup(popupToOpen.c_str());

        if (imguiMetricsOpen)
            ImGui::ShowMetricsWindow(&imguiMetricsOpen);

        drawModals();
        drawPopupNotifications();
        updateWindowTitle();

        entityEyedropperActive = false;
        handleOverrideEntity = entt::null;
    }
}
