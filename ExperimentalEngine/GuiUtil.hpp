#pragma once
#include <string>
#include "imgui.h"
#include <functional>
#include "imgui_stdlib.h"

// Open with ImGui::OpenPopup
void saveFileModal(const char* title, std::function<void(const char*)> saveCallback);