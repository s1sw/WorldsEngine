#include "Core/IGameEventHandler.hpp"
#include "EditorWindows.hpp"
#include <AssetCompilation/AssetCompilers.hpp>
#include <Editor/AssetEditors.hpp>
#include <Editor/Editor.hpp>
#include <ImGui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui_internal.h>

namespace worlds
{
    AssetEditorWindow::AssetEditorWindow(AssetID id, EngineInterfaces interfaces, Editor* editor)
        : EditorWindow(interfaces, editor), assetId(id), currCompileOp(nullptr)
    {
        IAssetEditorMeta* assetEditorMeta = AssetEditors::getEditorFor(id);
        assetEditor = assetEditorMeta->createEditorFor(id);
    }

    void AssetEditorWindow::draw(entt::registry& reg)
    {
        std::string path = AssetDB::idToPath(assetId);
        ImGui::SetNextWindowPos(glm::vec2(ImGui::GetMainViewport()->GetCenter()) - glm::vec2(640.0f, 480.0f) * 0.5f,
                                ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_Once);
        bool isUnsaved = false;

        if (assetEditor)
        {
            isUnsaved = assetEditor->hasUnsavedChanges();
        }

        if (ImGui::Begin(path.c_str(), &active, isUnsaved ? ImGuiWindowFlags_UnsavedDocument : 0))
        {
            if (ImGui::IsWindowFocused() && ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_ESCAPE] == 0.0f)
            {
                active = false;
            }

            if (assetId == INVALID_ASSET)
            {
                ImGui::Text("Invalid asset!");
            }
            else
            {
                assetEditor->draw();

                if (ImGui::Button("Save"))
                {
                    assetEditor->save();
                }

                if (currCompileOp)
                {
                    ImGui::ProgressBar(currCompileOp->progress);

                    if (currCompileOp->complete)
                    {
                        if (currCompileOp->result == CompilationResult::Error)
                        {
                            addNotification("Failed to compile asset", NotificationType::Error);
                        }

                        delete currCompileOp;
                        currCompileOp = nullptr;
                    }
                }
                else
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Compile"))
                    {
                        currCompileOp = AssetCompilers::buildAsset(editor->currentProject().root(), assetId);
                    }
                }
            }
        }
        ImGui::End();
    }

    AssetEditorWindow::~AssetEditorWindow()
    {
        assetEditor->save();
        delete assetEditor;
    }
}
