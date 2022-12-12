#include "Editor.hpp"
#include "Editor/EditorAssetSearchPopup.hpp"
#include "Editor/Widgets/IntegratedMenubar.hpp"
#include "GuiUtil.hpp"
#include "SDL_scancode.h"
#include <ComponentMeta/ComponentMetadata.hpp>
#include <Core/Engine.hpp>
#include <Core/Transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <memory>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "AssetEditors.hpp"
#include "EditorWindows/EditorWindows.hpp"
#include "ImGui/imgui_internal.h"
#include "Tracy.hpp"
#include <AssetCompilation/AssetCompilers.hpp>
#include <Audio/Audio.hpp>
#include <ComponentMeta/ComponentFuncs.hpp>
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <Core/Window.hpp>
#include <Editor/EditorActions.hpp>
#include <Editor/ProjectAssetCompiler.hpp>
#include <Editor/Widgets/EditorStartScreen.hpp>
#include <ImGui/ImGuizmo.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <Libs/pcg_basic.h>
#include <Physics/Physics.hpp>
#include <Physics/PhysicsActor.hpp>
#include <Scripting/NetVM.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Util/CreateModelObject.hpp>
#include <filesystem>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <nlohmann/json.hpp>
#include <slib/Subprocess.hpp>
#include <VR/OpenXRInterface.hpp>

namespace worlds
{
    slib::Subprocess* dotnetWatchProcess = nullptr;
    static ConVar ed_saveAsJson{"ed_saveAsJson", "0", "Save scene files as JSON rather than MessagePack."};

    const char* toolStr(Tool tool)
    {
        switch (tool)
        {
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

    std::string Editor::generateWindowTitle()
    {
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
            + std::string(project->name()) + " | " + reg.ctx<SceneInfo>().name;

        if (lastSaveModificationCount != undo.modificationCount())
        {
            base += "*";
        }

        return base;
    }

    void Editor::updateWindowTitle()
    {
        static std::string lastTitle;
        std::string newTitle = generateWindowTitle();

        if (lastTitle != newTitle)
        {
            interfaces.engine->getMainWindow().setTitle(newTitle.c_str());
        }
    }

    static int menuButtonsExtent = 0;

    EntityFolder::EntityFolder(std::string name) : name(name)
    {
        randomId = pcg32_random();
    }

    Editor::Editor(entt::registry& reg, EngineInterfaces& interfaces)
        : actionSearch(this, reg), assetSearch(this), currentTool(Tool::Translate), reg(reg),
          currentSelectedEntity(entt::null), lookX(0.0f), lookY(0.0f), cameraSpeed(5.0f), imguiMetricsOpen(false),
          settings(), interfaces(interfaces), inputManager(*interfaces.inputManager)
    {
        interfaces.engine->pauseSim = true;

#define ADD_EDITOR_WINDOW(type) editorWindows.add(std::make_unique<type>(interfaces, this))

        ADD_EDITOR_WINDOW(EntityList);
        ADD_EDITOR_WINDOW(Assets);
        ADD_EDITOR_WINDOW(GameControls);
        ADD_EDITOR_WINDOW(StyleEditor);
        ADD_EDITOR_WINDOW(AboutWindow);
        ADD_EDITOR_WINDOW(BakingWindow);
        ADD_EDITOR_WINDOW(SceneSettingsWindow);
        ADD_EDITOR_WINDOW(RawAssets);
        ADD_EDITOR_WINDOW(AssetCompilationManager);
        ADD_EDITOR_WINDOW(NodeEditorTest);
        ADD_EDITOR_WINDOW(GameView);

#undef ADD_EDITOR_WINDOW
        AssetCompilers::initialise();
        AssetEditors::initialise(interfaces);
        sceneViews.add(new EditorSceneView{interfaces, this});

        titleBarIcon = interfaces.renderer->getUITextureManager()->loadOrGet(
            AssetDB::pathToId("UI/Editor/Images/logo_no_background_small.png"));

        EntityFolders folders;

        reg.set<EntityFolders>(std::move(folders));
        loadOpenWindows();

        inputManager.addKeydownHandler(
            [&](SDL_Scancode scancode)
            {
                ModifierFlags flags = ModifierFlags::None;

                if (inputManager.ctrlHeld())
                    flags = flags | ModifierFlags::Control;

                if (inputManager.shiftHeld())
                    flags = flags | ModifierFlags::Shift;

                queuedKeydowns.add({scancode, flags});
            }
        );

        g_console->registerCommand(
            [&](const char*)
            {
                if (currentState != GameState::Editing)
                    return;

                interfaces.engine->pauseSim = false;

                if (reg.ctx<SceneInfo>().id != ~0u)
                    interfaces.engine->loadScene(reg.ctx<SceneInfo>().id);
                currentState = GameState::Playing;
            },
            "play", "play."
        );

        g_console->registerCommand(
            [&](const char*)
            {
                if (currentState != GameState::Playing)
                    return;

                interfaces.engine->pauseSim = true;
                interfaces.inputManager->lockMouse(false);
                addNotification("Scene paused");
                currentState = GameState::Paused;
            },
            "pauseAndEdit", "pause and edit."
        );

        g_console->registerCommand(
            [&](const char*)
            {
                if (reg.ctx<SceneInfo>().id != ~0u)
                    interfaces.engine->loadScene(reg.ctx<SceneInfo>().id);

                interfaces.engine->pauseSim = true;
                interfaces.inputManager->lockMouse(false);
                currentState = GameState::Editing;
            },
            "reloadAndEdit", "reload and edit."
        );

        g_console->registerCommand(
            [&](const char*)
            {
                interfaces.engine->pauseSim = false;
                currentState = GameState::Playing;
            },
            "unpause", "unpause and go back to play mode."
        );

        EditorActions::addAction({
            "scene.save",
            [&](Editor* ed, entt::registry& reg)
            {
                if (reg.ctx<SceneInfo>().id != ~0u && !inputManager.shiftHeld())
                {
                    AssetID sceneId = reg.ctx<SceneInfo>().id;
                    if (ed_saveAsJson.getInt())
                        JsonSceneSerializer::saveScene(sceneId, reg);
                    else
                        MessagePackSceneSerializer::saveScene(sceneId, reg);

                    lastSaveModificationCount = undo.modificationCount();
                }
                else
                {
                    ImGui::OpenPopup("Save Scene");
                }
            },
            "Save Scene"
        });

        EditorActions::addAction({
            "scene.open",
            [](Editor* ed, entt::registry& reg) { ImGui::OpenPopup("Open Scene"); },
            "Open Scene"
        });

        EditorActions::addAction({
            "scene.new",
            [](Editor* ed, entt::registry& reg)
            {
                entt::registry* regPtr = &reg;
                messageBoxModal("New Scene", "Are you sure you want to clear the current scene and create a new one?",
                                [ed, regPtr](bool result)
                                {
                                    if (result)
                                    {
                                        regPtr->clear();
                                        regPtr->set<SceneInfo>("Untitled", INVALID_ASSET);
                                        ed->updateWindowTitle();
                                    }
                                });
            },
            "New Scene"
        });

        EditorActions::addAction({
            "editor.undo",
            [](Editor* ed, entt::registry& reg) { ed->undo.undo(reg); },
            "Undo"
        });

        EditorActions::addAction({
            "editor.redo",
            [](Editor* ed, entt::registry& reg) { ed->undo.redo(reg); },
            "Redo"
        });

        EditorActions::addAction({
            "editor.togglePlay",
            [](Editor* ed, entt::registry& reg)
            {
                if (ed->isPlaying())
                    g_console->executeCommandStr("reloadAndEdit");
                else
                    g_console->executeCommandStr("play");
            },
            "Play"
        });

        EditorActions::addAction({
            "editor.togglePause",
            [](Editor* ed, entt::registry& reg)
            {
                GameState state = ed->getCurrentState();
                if (state == GameState::Paused)
                    g_console->executeCommandStr("unpause");
                else if (state == GameState::Playing)
                    g_console->executeCommandStr("pause");
            },
            "Toggle Pause"
        });

        EditorActions::addAction({
            "editor.toggleImGuiMetrics",
            [](Editor* ed, entt::registry&) { ed->imguiMetricsOpen = !ed->imguiMetricsOpen; },
            "Toggle ImGUI Metrics"
        });

        EditorActions::addAction({
            "editor.openActionSearch",
            [](Editor* ed, entt::registry&) { ed->actionSearch.show(); }
        });

        EditorActions::addAction({
            "editor.openAssetSearch",
            [](Editor* ed, entt::registry&) { ed->assetSearch.show(); }
        });

        EditorActions::addAction({
            "editor.addStaticPhysics",
            [](Editor* ed, entt::registry& reg)
            {
                if (!reg.valid(ed->currentSelectedEntity))
                {
                    addNotification("Nothing selected to add physics to!", NotificationType::Error);
                    return;
                }

                if (!reg.has<WorldObject>(ed->currentSelectedEntity))
                {
                    addNotification("Selected object doesn't have a WorldObject!", NotificationType::Error);
                    return;
                }

                WorldObject& wo = reg.get<WorldObject>(ed->currentSelectedEntity);
                ComponentMetadataManager::byName["Physics Actor"]->create(ed->currentSelectedEntity, reg);
                PhysicsActor& pa = reg.get<PhysicsActor>(ed->currentSelectedEntity);
                PhysicsShape ps;
                ps.type = PhysicsShapeType::Mesh;
                ps.mesh.mesh = wo.mesh;
                pa.physicsShapes.push_back(std::move(ps));
            },
            "Add static physics"
        });

        EditorActions::addAction({
            "editor.createPrefab",
            [](Editor*, entt::registry& reg) { ImGui::OpenPopup("Save Prefab"); },
            "Create prefab"
        });

        EditorActions::addAction({
            "editor.roundScale",
            [](Editor* ed, entt::registry& reg)
            {
                if (!reg.valid(ed->currentSelectedEntity))
                    return;
                Transform& t = reg.get<Transform>(ed->currentSelectedEntity);
                t.scale = glm::round(t.scale);

                for (entt::entity e : ed->selectedEntities)
                {
                    Transform& t = reg.get<Transform>(e);
                    t.scale = glm::round(t.scale);
                }
            },
            "Round selection scale"
        });

        EditorActions::addAction({
            "editor.clearScale",
            [](Editor* ed, entt::registry& reg)
            {
                if (!reg.valid(ed->currentSelectedEntity))
                    return;
                Transform& t = reg.get<Transform>(ed->currentSelectedEntity);
                t.scale = glm::vec3(1.0f);

                for (entt::entity e : ed->selectedEntities)
                {
                    Transform& t = reg.get<Transform>(e);
                    t.scale = glm::vec3(1.0f);
                }
            },
            "Clear selection scale"
        });

        EditorActions::addAction({
            "editor.setStatic",
            [](Editor* ed, entt::registry& reg)
            {
                if (!reg.valid(ed->currentSelectedEntity))
                    return;

                StaticFlags allFlags =
                    StaticFlags::Audio |
                    StaticFlags::Rendering |
                    StaticFlags::Navigation;

                reg.get<WorldObject>(ed->currentSelectedEntity).staticFlags = allFlags;
                for (entt::entity e : ed->getSelectedEntities())
                {
                    reg.get<WorldObject>(e).staticFlags = allFlags;
                }
            },
            "Set selected object as static"
        });

        EditorActions::addAction({
            "editor.copy",
            [](Editor* ed, entt::registry& reg)
            {
                if (!reg.valid(ed->currentSelectedEntity)) return;
                std::vector<entt::entity> allSelectedEntities;
                allSelectedEntities.push_back(ed->currentSelectedEntity);
                for (entt::entity e : ed->selectedEntities)
                {
                    allSelectedEntities.push_back(e);
                }

                std::string entityJson =
                    JsonSceneSerializer::entitiesToJson(reg, allSelectedEntities.data(), allSelectedEntities.size());
                SDL_SetClipboardText(entityJson.c_str());
            },
            "Copy selected entities"
        });

        EditorActions::addAction({
            "editor.paste",
            [](Editor* ed, entt::registry& reg)
            {
                if (!SDL_HasClipboardText()) return;
                const char* txt = SDL_GetClipboardText();
                try
                {
                    auto pastedEnts = JsonSceneSerializer::jsonToEntities(reg, txt);
                    if (!pastedEnts.empty())
                    {
                        ed->select(pastedEnts[0]);

                        for (size_t i = 1; i < pastedEnts.size(); i++)
                        {
                            ed->multiSelect(pastedEnts[i]);
                        }
                    }
                    addNotification("Entity pasted! :)");
                }
                catch (nlohmann::detail::exception& e)
                {
                    logErr("Failed to deserialize clipboard entity: %s", e.what());
                    addNotification("Sorry, we couldn't paste that into the scene.", NotificationType::Error);
                }
            }
        });

        EditorActions::addAction({
            "assets.refresh",
            [](Editor* ed, entt::registry& reg)
            {
                ed->currentProject().assets().enumerateAssets();
                ed->currentProject().assets().checkForAssetChanges();
                ed->currentProject().assetCompiler().startCompiling();
            },
            "Refresh assets"
        });

        struct ActionBindingPair
        {
            const char* actionName;
            SDL_Scancode scancode;
            ModifierFlags flags;
        };

        ActionBindingPair bindingPairs[] = {
            {"scene.save", SDL_SCANCODE_S, ModifierFlags::Control},
            {"scene.open", SDL_SCANCODE_O, ModifierFlags::Control},
            {"scene.new", SDL_SCANCODE_N, ModifierFlags::Control},
            {"editor.undo", SDL_SCANCODE_Z, ModifierFlags::Control},
            {"editor.redo", SDL_SCANCODE_Z, ModifierFlags::Control | ModifierFlags::Shift},
            {"editor.togglePlay", SDL_SCANCODE_P, ModifierFlags::Control},
            {"editor.togglePause", SDL_SCANCODE_P, ModifierFlags::Control | ModifierFlags::Shift},
            {"editor.openActionSearch", SDL_SCANCODE_SPACE, ModifierFlags::Control},
            {"editor.openAssetSearch", SDL_SCANCODE_SPACE, ModifierFlags::Control | ModifierFlags::Shift},
            {"editor.copy", SDL_SCANCODE_C, ModifierFlags::Control},
            {"editor.paste", SDL_SCANCODE_V, ModifierFlags::Control}
        };

        for (auto& bindPair : bindingPairs)
        {
            ActionKeybind kb{bindPair.scancode, bindPair.flags};
            EditorActions::bindAction(bindPair.actionName, kb);
        }

        SceneLoader::registerLoadCallback(this, sceneLoadCallback);
    }

    Editor::~Editor()
    {
        saveOpenWindows();

        if (dotnetWatchProcess)
        {
            dotnetWatchProcess->kill();
            delete dotnetWatchProcess;
        }
    }

    void Editor::select(entt::entity entity)
    {
        // Remove selection from existing entity
        if (reg.valid(currentSelectedEntity) && reg.has<EditorGlow>(currentSelectedEntity))
        {
            reg.remove<EditorGlow>(currentSelectedEntity);
        }

        currentSelectedEntity = entity;
        // A null entity means we should deselect the current entity
        for (auto ent : selectedEntities)
        {
            if (reg.valid(ent))
                reg.remove_if_exists<EditorGlow>(ent);
        }
        selectedEntities.clear();

        if (!reg.valid(entity))
        {
            return;
        }

        reg.emplace<EditorGlow>(currentSelectedEntity);
    }

    void Editor::multiSelect(entt::entity entity)
    {
        if (!reg.valid(entity))
            return;

        if (!reg.valid(currentSelectedEntity))
        {
            select(entity);
            return;
        }

        if (entity == currentSelectedEntity)
        {
            if (selectedEntities.numElements() == 0)
            {
                select(entt::null);
            }
            else
            {
                select(selectedEntities[0]);
                selectedEntities.removeAt(0);
            }
            return;
        }

        if (selectedEntities.contains(entity))
        {
            selectedEntities.removeValue(entity);
            reg.remove_if_exists<EditorGlow>(entity);
        }
        else
        {
            reg.emplace<EditorGlow>(entity);
            selectedEntities.add(entity);
        }
    }

    ImVec2 convVec(glm::vec2 gVec)
    {
        return {gVec.x, gVec.y};
    }

    glm::vec2 convVec(ImVec2 iVec)
    {
        return {iVec.x, iVec.y};
    }

    void Editor::activateTool(Tool newTool)
    {
        assert(reg.valid(currentSelectedEntity));
        currentTool = newTool;
        originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
    }

    bool Editor::isEntitySelected(entt::entity ent) const
    {
        if (currentSelectedEntity == ent)
            return true;

        for (entt::entity selectedEnt : selectedEntities)
        {
            if (selectedEnt == ent)
                return true;
        }

        return false;
    }

    ImGuizmo::OPERATION toolToOp(Tool t)
    {
        switch (t)
        {
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

    void Editor::overrideHandle(Transform* t)
    {
        handleOverriden = true;
        overrideTransform = t;
    }

    void Editor::overrideHandle(entt::entity e)
    {
        handleOverrideEntity = e;
    }

    bool Editor::entityEyedropper(entt::entity& picked)
    {
        entityEyedropperActive = true;

        if (reg.valid(eyedroppedEntity))
        {
            picked = eyedroppedEntity;
            eyedroppedEntity = entt::null;
            entityEyedropperActive = false;
            return true;
        }

        return false;
    }

    void Editor::saveOpenWindows()
    {
        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::string openWindowPath = prefPath + std::string("openWindows.json");

        nlohmann::json j;

        for (auto& window : editorWindows)
        {
            j[window->getName()] = {{"open", window->isActive()}};
        }

        std::ofstream o{openWindowPath};
        o << std::setw(2) << j;
        o.close();
    }

    void Editor::loadOpenWindows()
    {
        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::string openWindowPath = prefPath + std::string("openWindows.json");

        nlohmann::json j;
        std::ifstream i{openWindowPath};

        if (!i.good())
            return;

        i >> j;
        i.close();

        for (auto& p : j.items())
        {
            std::string name = p.key();
            for (auto& win : editorWindows)
            {
                if (win->getName() == name)
                {
                    win->setActive(p.value().value("open", win->isActive()));
                }
            }
        }
    }

    EditorSceneView* Editor::getFirstSceneView()
    {
        return sceneViews[0];
    }

    void Editor::openAsset(AssetID id)
    {
        if (AssetDB::getAssetExtension(id) == ".wscn")
        {
            interfaces.engine->loadScene(id);
            updateWindowTitle();
            undo.clear();
            return;
        }
        else if (AssetDB::getAssetExtension(id) == ".wprefab")
        {
            entt::entity ent = SceneLoader::createPrefab(id, reg);
            if (getFirstSceneView())
            {
                float dist = 1.0f;
                if (reg.has<WorldObject>(ent))
                {
                    WorldObject& wo = reg.get<WorldObject>(ent);
                    const LoadedMesh& lm = MeshManager::loadOrGet(wo.mesh);
                    dist = lm.sphereBoundRadius + 1.0f;
                }

                Transform& t = reg.get<Transform>(ent);
                Camera& cam = getFirstSceneView()->getCamera();
                t.position = cam.position + cam.rotation * glm::vec3(0.0f, 0.0f, dist);
            }
            return;
        }

        AssetEditorWindow* editor = new AssetEditorWindow(id, interfaces, this);
        editor->setActive(true);
        assetEditors.add(editor);
    }

    void Editor::eyedropperSelect(entt::entity picked)
    {
        eyedroppedEntity = picked;
    }

    void Editor::handleTools(Transform& t, ImVec2 wPos, ImVec2 wSize, Camera& camera)
    {
        // Convert selected transform position from world space to screen space
        glm::vec4 ndcObjPosPreDivide =
            camera.getProjectionMatrix((float)wSize.x / wSize.y) * camera.getViewMatrix() * glm::vec4(t.position, 1.0f);

        // NDC -> screen space
        glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
        ndcObjectPosition /= ndcObjPosPreDivide.w;
        ndcObjectPosition *= 0.5f;
        ndcObjectPosition += 0.5f;
        ndcObjectPosition *= convVec(wSize);
        // Not sure why flipping Y is necessary?
        ndcObjectPosition.y = wSize.y - ndcObjectPosition.y;

        if ((ndcObjPosPreDivide.z / ndcObjPosPreDivide.w) > 0.0f)
            ImGui::GetWindowDrawList()->AddCircleFilled(convVec(ndcObjectPosition) + wPos, 7.0f,
                                                        ImColor(0.0f, 0.0f, 0.0f));

        ImGuizmo::BeginFrame();
        ImGuizmo::Enable(true);
        ImGuizmo::SetRect(wPos.x, wPos.y, (float)wSize.x, (float)wSize.y);
        ImGuizmo::SetDrawlist();

        glm::mat4 view = camera.getViewMatrix();
        // Get a relatively normal projection matrix so ImGuizmo doesn't break.
        glm::mat4 proj = camera.getProjectionMatrixZONonInfinite((float)wSize.x / (float)wSize.y);

        glm::mat4 tfMtx = t.getMatrix();
        glm::vec3 snap{0.0f};

        static glm::vec3 boundsScale{1.0f};

        if (inputManager.keyHeld(SDL_SCANCODE_LCTRL, true))
        {
            switch (currentTool)
            {
            case Tool::Rotate:
                snap = glm::vec3{settings.angularSnapIncrement};
                break;
            case Tool::Bounds:
                snap = boundsScale; // glm::vec3{ settings.snapIncrement } / t.scale;
                break;
            default:
                snap = glm::vec3{settings.snapIncrement};
                break;
            }
        }

        static float bounds[6]{-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

        glm::mat4 deltaMatrix{1.0f};

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), toolToOp(currentTool),
                             toolLocalSpace ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, glm::value_ptr(tfMtx),
                             glm::value_ptr(deltaMatrix), glm::value_ptr(snap),
                             currentTool == Tool::Bounds ? bounds : nullptr, glm::value_ptr(snap));

        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(tfMtx, scale, rotation, translation, skew, perspective);

        static bool usingLast = false;
        if (!usingLast && ImGuizmo::IsUsing())
        {
            undo.pushState(reg);
            boundsScale = glm::vec3{settings.snapIncrement} / t.scale;
        }

        usingLast = ImGuizmo::IsUsing();

        switch (currentTool)
        {
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

        if (!handleOverriden)
        {
            for (auto ent : selectedEntities)
            {
                if (!reg.has<ChildComponent>(ent))
                {
                    auto& msTransform = reg.get<Transform>(ent);
                    msTransform.fromMatrix(deltaMatrix * msTransform.getMatrix());
                }
                else
                {
                    auto& cc = reg.get<ChildComponent>(ent);
                    cc.offset.fromMatrix(deltaMatrix * cc.offset.getMatrix());
                }
            }
        }
    }

    ConVar ed_runDotNetWatch{"ed_runDotNetWatch", "0"};

    void Editor::openProject(std::string path)
    {
        ZoneScoped;

        project = std::make_unique<GameProject>(path);
        project->mountPaths();
        // interfaces.renderer->reloadContent(ReloadFlags::All);

        // Update recent projects list
        std::vector<std::string> recentProjects;

        char* prefPath = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::ifstream recentProjectsStream(prefPath + std::string{"recentProjects.txt"});

        if (recentProjectsStream.good())
        {
            std::string currLine;

            while (std::getline(recentProjectsStream, currLine))
            {
                recentProjects.push_back(currLine);
            }
        }

        recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), path), recentProjects.end());
        recentProjects.insert(recentProjects.begin(), path);

        std::ofstream recentProjectsOutStream(prefPath + std::string{"recentProjects.txt"});

        if (recentProjectsOutStream.good())
        {
            for (std::string& path : recentProjects)
            {
                recentProjectsOutStream << path << "\n";
            }
        }

        SDL_free(prefPath);

        AudioSystem::getInstance()->loadMasterBanks();
        if (gameProjectSelectedCallback == nullptr)
            interfaces.scriptEngine->createManagedDelegate("WorldsEngine.Editor.Editor", "OnGameProjectSelected",
                                                           (void**)&gameProjectSelectedCallback);
        gameProjectSelectedCallback(project.get());

        if (interfaces.vrInterface && PHYSFS_exists("SourceData/VRInput/actions.json"))
        {
            interfaces.vrInterface->loadActionJson("SourceData/VRInput/actions.json");
        }

        if (ed_runDotNetWatch)
        {
            dotnetWatchProcess =
                new slib::Subprocess("dotnet watch build",
                                     (std::string(project->root()) + "/Code").c_str());
        }
    }

    void Editor::update(float deltaTime)
    {
        ZoneScoped;
        static IntegratedMenubar menubar{interfaces};

        auto sceneViewRemove = [](EditorSceneView* esv)
        {
            if (!esv->open)
            {
                delete esv;
                return true;
            }
            return false;
        };

        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), sceneViewRemove), sceneViews.end());

        auto aeRemove = [](AssetEditorWindow* ae)
        {
            if (!ae->isActive())
            {
                delete ae;
                return true;
            }

            return false;
        };

        assetEditors.erase(std::remove_if(assetEditors.begin(), assetEditors.end(), aeRemove), assetEditors.end());

        bool anyFocused = false;

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            for (ImGuiViewport* v : ImGui::GetPlatformIO().Viewports)
            {
                bool focus = ImGui::GetPlatformIO().Platform_GetWindowFocus(v);
                if (focus)
                    anyFocused = true;
            }
        }
        else
        {
            anyFocused = interfaces.engine->getMainWindow().isFocused();
        }

        for (EditorSceneView* esv : sceneViews)
        {
            esv->setViewportActive(anyFocused && project);
        }

        if (!isPlaying())
            AudioSystem::getInstance()->stopEverything(reg);

        if (!project)
        {
            updateWindowTitle();

            ImVec2 menuBarSize;
            if (ImGui::BeginMainMenuBar())
            {
                if (g_console->getConVar("ed_integratedmenubar")->getInt())
                {
                    float menuBarHeight = ImGui::GetWindowHeight();
                    float spacing = (menuBarHeight - 24) / 2.0f;
                    float prevCursorY = ImGui::GetCursorPosY();
                    ImGui::SetCursorPosY(prevCursorY + spacing);
                    ImGui::Image(titleBarIcon, ImVec2(24, 24));
                    ImGui::SetCursorPosY(prevCursorY);
                }

                menuButtonsExtent = 24;

                menuBarSize = ImGui::GetWindowSize();
                menubar.draw();
                ImGui::EndMainMenuBar();
            }

            static EditorStartScreen startScreen{interfaces};
            startScreen.draw(this);
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
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleVar(3);
        ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0, 0));
        ImGui::End();

        // Draw black background
        ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos, viewport->Size, ImColor(0.0f, 0.0f, 0.0f, 1.0f));

        if (reg.valid(currentSelectedEntity))
        {
            // Right mouse button means that the view's being moved, so we'll ignore any tools
            // and assume the user's trying to move the camera
            if (!inputManager.mouseButtonHeld(MouseButton::Right, true))
            {
                if (inputManager.keyPressed(SDL_SCANCODE_G))
                {
                    activateTool(Tool::Translate);
                }
                else if (inputManager.keyPressed(SDL_SCANCODE_R))
                {
                    activateTool(Tool::Rotate);
                }
                else if (inputManager.keyPressed(SDL_SCANCODE_S))
                {
                    activateTool(Tool::Scale);
                }
                else if (inputManager.keyPressed(SDL_SCANCODE_B))
                {
                    activateTool(Tool::Bounds);
                }
            }
        }

        if (ImGui::Begin(ICON_FA_EDIT u8" Editor"))
        {
            ImGui::Text("Modification Count: %u", undo.modificationCount());
            if (ImGui::Button(ICON_FA_UNDO u8" Undo"))
            {
                undo.undo(reg);
            }

            ImGui::SameLine();

            if (ImGui::Button(ICON_FA_REDO u8" Redo"))
            {
                undo.redo(reg);
            }

            ImGui::Text("Current tool: %s", toolStr(currentTool));

            ImGui::Checkbox("Manipulate in local space", &toolLocalSpace);

            ImGui::Checkbox("Global object snap", &settings.objectSnapGlobal);
            tooltipHover("If this is checked, moving an object with Ctrl held will snap in increments relative to the "
                "world rather than the object's original position.");

            if (ImGui::CollapsingHeader("Physics Simulation"))
            {
                ImGui::Checkbox("Pause physics", &interfaces.engine->pauseSim);

                if (ImGui::Button("Disable rigidbodies"))
                {
                    reg.view<RigidBody>().each([](RigidBody& rb)
                    {
                        rb.actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                    });
                }

                ImGui::SameLine();

                if (ImGui::Button("Enable rigidbodies"))
                {
                    reg.view<RigidBody>().each([](RigidBody& rb)
                    {
                        rb.actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                    });
                }

                if (ImGui::Button("Enable Selected"))
                {
                    auto f = [&](entt::entity ent)
                    {
                        if (!reg.valid(ent)) return;

                        RigidBody* rb = reg.try_get<RigidBody>(ent);
                        if (rb)
                        {
                            rb->actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
                            rb->actor->wakeUp();
                        }
                    };

                    f(getSelectedEntity());
                    for (entt::entity ent : getSelectedEntities())
                    {
                        f(ent);
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Disable Selected"))
                {
                    auto f = [&](entt::entity ent)
                    {
                        if (!reg.valid(ent)) return;

                        RigidBody* rb = reg.try_get<RigidBody>(ent);
                        if (rb)
                        {
                            rb->actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                        }
                    };

                    f(getSelectedEntity());
                    for (entt::entity ent : getSelectedEntities())
                    {
                        f(ent);
                    }
                }
            }

            ImGui::InputFloat("Snap increment", &settings.snapIncrement, 0.1f, 0.5f);
            ImGui::InputFloat("Angular snap increment", &settings.angularSnapIncrement, 0.5f, 1.0f);
            ImGui::InputFloat("Camera speed", &cameraSpeed, 0.1f);

            ImGui::InputFloat("Scene icon distance", &settings.sceneIconDrawDistance);
        }
        ImGui::End();

        for (auto& edWindow : editorWindows)
        {
            if (edWindow->isActive())
            {
                edWindow->draw(reg);
            }
        }

        for (AssetEditorWindow* ae : assetEditors)
        {
            ae->draw(reg);
        }

        saveFileModal("Save Scene", [this](const char* path)
        {
            AssetID sceneId = AssetDB::createAsset(path);
            JsonSceneSerializer::saveScene(sceneId, reg);
            lastSaveModificationCount = undo.modificationCount();
            interfaces.engine->loadScene(sceneId);
        });

        const char* sceneFileExts[] = {".wscn"};

        openFileModalOffset(
            "Open Scene",
            [this](const char* path)
            {
                interfaces.engine->loadScene(AssetDB::pathToId(path));
                updateWindowTitle();
                undo.clear();
            },
            "SourceData/", sceneFileExts, 1);

        std::string popupToOpen;

        if (ImGui::BeginMainMenuBar())
        {
            if (g_console->getConVar("ed_integratedmenubar")->getInt())
            {
                float menuBarHeight = ImGui::GetWindowHeight();
                float spacing = (menuBarHeight - 24) / 2.0f;
                float prevCursorY = ImGui::GetCursorPosY();
                ImGui::SetCursorPosY(prevCursorY + spacing);
                ImGui::Image(titleBarIcon, ImVec2(24, 24));
                ImGui::SetCursorPosY(prevCursorY);
            }

            if (ImGui::BeginMenu("File"))
            {
                for (auto& window : editorWindows)
                {
                    if (window->menuSection() == EditorMenu::File)
                    {
                        if (ImGui::MenuItem(window->getName()))
                        {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                if (ImGui::MenuItem("New", "Ctrl+N"))
                {
                    EditorActions::findAction("scene.new").function(this, reg);
                }

                if (ImGui::MenuItem("Open", "Ctrl+O"))
                {
                    EditorActions::findAction("scene.open").function(this, reg);
                }

                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    EditorActions::findAction("scene.save").function(this, reg);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Close Project"))
                {
                    project->unmountPaths();
                    project.reset();

                    reg.clear();
                    reg.set<SceneInfo>("Untitled", INVALID_ASSET);
                    // interfaces.renderer->reloadContent(worlds::ReloadFlags::All);

                    if (dotnetWatchProcess)
                    {
                        dotnetWatchProcess->kill();
                        delete dotnetWatchProcess;
                    }

                    if (gameProjectClosedCallback == nullptr)
                    {
                        interfaces.scriptEngine->createManagedDelegate(
                            "WorldsEngine.Editor.Editor", "OnGameProjectClosed", (void**)&gameProjectClosedCallback);
                    }

                    gameProjectClosedCallback();
                }

                if (ImGui::MenuItem("Quit"))
                {
                    interfaces.engine->quit();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                {
                    undo.undo(reg);
                }

                if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z"))
                {
                    undo.redo(reg);
                }

                ImGui::Separator();

                for (auto& window : editorWindows)
                {
                    if (window->menuSection() == EditorMenu::Edit)
                    {
                        if (ImGui::MenuItem(window->getName()))
                        {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("New Scene View"))
                {
                    sceneViews.add(new EditorSceneView{interfaces, this});
                }

                if (ImGui::MenuItem("Create Prefab", nullptr, false, reg.valid(currentSelectedEntity)))
                {
                    popupToOpen = "Save Prefab";
                }

                if (ImGui::MenuItem("Add Static Physics", nullptr, false,
                                    reg.valid(currentSelectedEntity) && reg.has<WorldObject>(currentSelectedEntity)))
                {
                    EditorActions::findAction("editor.addStaticPhysics").function(this, reg);
                }

                if (ImGui::MenuItem("Pause Asset Watcher", nullptr, &project->assets().pauseWatcher, true))
                {
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                for (auto& window : editorWindows)
                {
                    if (window->menuSection() == EditorMenu::Window)
                    {
                        if (ImGui::MenuItem(window->getName()))
                        {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                for (auto& window : editorWindows)
                {
                    if (window->menuSection() == EditorMenu::Help)
                    {
                        if (ImGui::MenuItem(window->getName()))
                        {
                            window->setActive(!window->isActive());
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (project && project->assetCompiler().isCompiling())
            {
                AssetCompileOperation* currentOp = project->assetCompiler().currentOperation();
                if (currentOp)
                {
                    std::filesystem::path filename = std::filesystem::path(AssetDB::idToPath(currentOp->outputId)).
                        filename();
                    ImGui::Text("Compiling %s", filename.string().c_str());
                    ImGui::ProgressBar(currentOp->progress, ImVec2(150.0f, 0.0f));
                }
            }

            menuButtonsExtent = (int)ImGui::GetCursorPosX();

            menubar.draw();

            ImGui::EndMainMenuBar();
        }

        saveFileModal("Save Prefab", [&](const char* path)
        {
            PHYSFS_File* file = PHYSFS_openWrite(path);
            JsonSceneSerializer::saveEntity(file, reg, currentSelectedEntity);
        });

        interfaces.scriptEngine->onEditorUpdate(deltaTime);

        int sceneViewIndex = 0;
        for (EditorSceneView* sceneView : sceneViews)
        {
            sceneView->drawWindow(sceneViewIndex++);
        }

        actionSearch.draw();
        assetSearch.draw();

        if (!popupToOpen.empty())
            ImGui::OpenPopup(popupToOpen.c_str());

        if (imguiMetricsOpen)
            ImGui::ShowMetricsWindow(&imguiMetricsOpen);

        drawModals();
        drawPopupNotifications();
        updateWindowTitle();

        for (QueuedKeydown& qd : queuedKeydowns)
        {
            EditorActions::triggerBoundActions(this, reg, qd.scancode, qd.modifiers);
        }
        queuedKeydowns.clear();
        EditorActions::reenable();

        if (project)
        {
            if (project->assets().recompileFlag)
            {
                project->assetCompiler().startCompiling();
                project->assets().recompileFlag = false;
            }
            project->assetCompiler().updateCompilation();
            
            if (ed_runDotNetWatch)
            {
                if (dotnetWatchProcess == nullptr)
                    dotnetWatchProcess =
                        new slib::Subprocess("dotnet watch build",
                                             (std::string(project->root()) + "/Code").c_str());
            }
            else if (dotnetWatchProcess != nullptr)
            {
                dotnetWatchProcess->kill();
                delete dotnetWatchProcess;
                dotnetWatchProcess = nullptr;
            }
        }

        entityEyedropperActive = false;
        handleOverrideEntity = entt::null;
    }

    void Editor::sceneLoadCallback(void* ctx, entt::registry& reg)
    {
        Editor* _this = (Editor*)ctx;
        _this->select(entt::null);
    }
}
