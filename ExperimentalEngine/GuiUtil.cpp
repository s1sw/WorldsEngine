#include "GuiUtil.hpp"
#include "Engine.hpp"
#include "IconsFontAwesome5.h"
#include "IconsFontaudio.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include <filesystem>

namespace worlds {
    void saveFileModal(const char* title, std::function<void(const char*)> saveCallback) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2) - (popupSize.x / 2), (windowSize.y / 2) - (popupSize.y / 2)));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* pathPtr = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
            std::string* saveDir = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (pathPtr == nullptr) {
                pathPtr = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), pathPtr);
            }

            if (saveDir == nullptr) {
                saveDir = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), saveDir);
            }

            ImGui::Text("%s", saveDir->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);
            char** files = PHYSFS_enumerateFiles(saveDir->c_str());

            if (!saveDir->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*saveDir);
                    *saveDir = p.parent_path().string();
                }
            }

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                PHYSFS_Stat stat;
                PHYSFS_stat(*currFile, &stat);
                ImGui::Text("%s%s", *currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *pathPtr = *currFile;
                    } else {
                        *saveDir = *currFile;
                    }
                }
            }
            ImGui::EndChild();

            std::string titleConfirm = "Confirm##";
            titleConfirm += title;

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", pathPtr);

            std::filesystem::path filePath = *saveDir;
            filePath.append(*pathPtr);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !pathPtr->empty()) {
                if (PHYSFS_exists(filePath.string().c_str())) {
                    ImGui::OpenPopup(titleConfirm.c_str());
                } else {
                    saveCallback(filePath.string().c_str());
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            if (ImGui::BeginPopup(titleConfirm.c_str())) {
                ImGui::Text("This file already exists. Do you want to overwrite?");

                if (ImGui::Button("Overwrite")) {
                    saveCallback(filePath.string().c_str());
                    ImGui::ClosePopupToLevel(0, true);
                }
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }
    }

    void openFileModal(const char* title, std::function<void(const char*)> openCallback, const char* fileExtension) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2) - (popupSize.x / 2), (windowSize.y / 2) - (popupSize.y / 2)));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* pathPtr = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
            std::string* saveDir = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (pathPtr == nullptr) {
                pathPtr = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), pathPtr);
            }

            if (saveDir == nullptr) {
                saveDir = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), saveDir);
            }

            ImGui::Text("%s", saveDir->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);
            char** files = PHYSFS_enumerateFiles(saveDir->c_str());

            std::filesystem::path filePath = *saveDir;
            filePath.append(*pathPtr);

            std::string titleDoesntExist = "File Doesn't Exist##";
            titleDoesntExist += title;

            if (!saveDir->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*saveDir);
                    *saveDir = p.parent_path().string();
                }
            }

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                PHYSFS_Stat stat;
                PHYSFS_stat(*currFile, &stat);
                std::string extension = std::filesystem::path(*currFile).extension().string();

                if (fileExtension && stat.filetype == PHYSFS_FILETYPE_REGULAR && extension != fileExtension)
                    continue;

                const char* icon = "  ";

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    icon = (const char*)(ICON_FA_FOLDER u8" ");
                } else if (extension == ".escn") {
                    icon = (const char*)(ICON_FA_MAP u8" " );
                } else if (extension == ".ogg") {
                    icon = (const char*)(ICON_FAD_SPEAKER u8" " );
                } else if (extension == ".crn") {
                    icon = (const char*)(ICON_FA_IMAGE u8" " );
                } else if (extension == ".obj" || extension == ".wmdl" || extension == ".mdl") {
                    icon = (const char*)(ICON_FA_SHAPES u8" " );
                }
                
                ImGui::Text("%s%s%s", icon, *currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *pathPtr = *currFile;
                    } else {
                        *saveDir = *currFile;
                    }
                }

                if (ImGui::IsItemHovered()) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        std::filesystem::path p(*saveDir);
                        *saveDir = p.parent_path().string();

                        if (!PHYSFS_exists(filePath.string().c_str())) {
                            ImGui::OpenPopup(titleDoesntExist.c_str());
                        } else {
                            openCallback(filePath.string().c_str());
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", pathPtr);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !pathPtr->empty()) {
                if (!PHYSFS_exists(filePath.string().c_str())) {
                    ImGui::OpenPopup(titleDoesntExist.c_str());
                } else {
                    openCallback(filePath.string().c_str());
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            if (ImGui::BeginPopup(titleDoesntExist.c_str())) {
                ImGui::Text("File not found.");
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }
    }

    bool selectAssetPopup(const char* title, AssetID& id, bool open) {
        static std::string path;
        static bool notFoundErr = false;
        bool changed = false;

        if (ImGui::BeginPopup(title)) {
            ImGui::InputText("Path", &path);

            if (notFoundErr) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not found");
            }

            if (ImGui::Button("Select")) {
                if (PHYSFS_exists(path.c_str())) {
                    id = g_assetDB.addOrGetExisting(path);
                    changed = true;
                    ImGui::CloseCurrentPopup();
                    notFoundErr = false;
                } else {
                    notFoundErr = true;
                }
            }

            ImGui::EndPopup();
        }

        if (open) {
            path = g_assetDB.getAssetPath(id);
            ImGui::OpenPopup(title);
        }

        return changed;
    }
}
