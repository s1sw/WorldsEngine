#include "GuiUtil.hpp"
#include "Engine.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

void saveFileModal(const char* title, std::function<void(const char*)> saveCallback) {
    ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2) - (popupSize.x / 2), (windowSize.y / 2) - (popupSize.y / 2)));
    ImGui::SetNextWindowSize(popupSize);

    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        std::string* pathPtr = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
        if (pathPtr == nullptr) {
            pathPtr = new std::string;
            ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), pathPtr);
        }

        ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 75.0f), true);
        char** files = PHYSFS_enumerateFiles("/");

        for (char** currFile = files; *currFile != nullptr; currFile++) {
            ImGui::Text("%s", *currFile);

            if (ImGui::IsItemHovered()) {

            }

            if (ImGui::IsItemClicked()) {
                printf("click\n");
            }
        }
        ImGui::EndChild();

        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
        ImGui::InputText("", pathPtr);

        ImGui::SameLine();
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}
