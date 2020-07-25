#include "Editor.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include <glm/gtx/norm.hpp>
#include "ImGuizmo.h"
#include "2DClip.hpp"
#undef near
#undef far

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
    }
}

Editor::Editor(entt::registry& reg, InputManager& inputManager, Camera& cam)
    : reg(reg)
    , inputManager(inputManager)
    , cam(cam)
    , currentTool(Tool::None)
    , currentAxisLock(AxisFlagBits::All) {
}

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

glm::vec3 projectDir(glm::vec3 dir, Camera& cam) {
    return cam.getProjectionMatrix(1280.0f / 720.0f) * glm::vec4(cam.rotation * dir, 0.0f);
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

float signedDistance(glm::vec2 a, glm::vec2 b) {
    glm::vec2 dif = a - b;
    float length2 = dif.x + dif.y;
    float sign = glm::sign(length2);
    return glm::sqrt(glm::abs(length2)) * sign;
}

int getCircleSegments(float radius) {
    return glm::max((int)glm::pow(radius, 0.8f), 6);
}

void Editor::update() {
    if (currentTool == Tool::None && reg.valid(currentSelectedEntity)) {
        // Right mouse button means that the view's being moved, so we'll need the movement keys
        if (!inputManager.mouseButtonHeld(MouseButton::Right)) {
            if (inputManager.keyPressed(SDL_SCANCODE_G)) {
                currentTool = Tool::Translate;
            } else if (inputManager.keyPressed(SDL_SCANCODE_R)) {
                currentTool = Tool::Rotate;
            } else if (inputManager.keyPressed(SDL_SCANCODE_S)) {
                currentTool = Tool::Scale;
            }
            originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
            currentAxisLock = AxisFlagBits::All;
            startingMouseDistance = -1.0f;

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

        ImGui::Checkbox("Global object snap", &objectSnapGlobal);
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

            glm::vec3 newPos;

            float d = -glm::dot(originalObjectTransform.position, n);


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


            float t = -(glm::dot(cam.position, n) + d) / glm::dot(dir, n);

            glm::vec3 dif = (cam.position + dir * t) - originalObjectTransform.position;

            if (inputManager.keyHeld(SDL_SCANCODE_LCTRL) && !objectSnapGlobal)
                dif = glm::round(dif);

            selectedTransform.position = originalObjectTransform.position + filterAxes(dif, currentAxisLock);

            if (inputManager.keyHeld(SDL_SCANCODE_LCTRL) && objectSnapGlobal)
                selectedTransform.position = glm::round(selectedTransform.position);
            
            glm::mat4 vp = cam.getProjectionMatrix((float)windowSize.x / windowSize.y)* cam.getViewMatrix();

            if (getNumActiveAxes(currentAxisLock) == 2) {
                ImGui::GetForegroundDrawList()->AddLine(worldToScreen(glm::vec3(0.0f), vp), worldToScreen(n, vp), ImColor(1.0f, 1.0f, 1.0f), 2.0f);
            } else if (getNumActiveAxes(currentAxisLock) == 1) {
                glm::vec3 x(1.0f, 0.0f, 0.0f);
                glm::vec3 y(0.0f, 1.0f, 0.0f);
                glm::vec3 z(0.0f, 0.0f, 1.0f);

                x *= hasAxis(currentAxisLock, AxisFlagBits::X);
                y *= hasAxis(currentAxisLock, AxisFlagBits::Y);
                z *= hasAxis(currentAxisLock, AxisFlagBits::Z);

                glm::vec3 start = originalObjectTransform.position - (x + y + z) * 100.0f;
                glm::vec3 end = originalObjectTransform.position + (x + y + z) * 100.0f;

                glm::vec2 startScreen = worldToScreenG(start, vp);
                glm::vec2 endScreen = worldToScreenG(end, vp);

                // clipping
                bool accept = lineClip(startScreen, endScreen, glm::vec2(0.0f), windowSize);

                ImGui::GetForegroundDrawList()->AddLine(glmToImgui(startScreen), glmToImgui(endScreen), ImColor(1.0f, 1.0f, 1.0f), 2.0f);

                ImGui::Text("Start: %f, %f, %f", start.x, start.y, start.z);
                ImGui::Text("End: %f, %f, %f", end.x, end.y, end.z);
                ImGui::Text("Start screen: %f, %f", startScreen.x, startScreen.y);
                ImGui::Text("End screen: %f, %f", endScreen.x, endScreen.y);
                ImGui::Text("Accept: %i", accept);
            }


        } else if (currentTool == Tool::Rotate) {

        } else if (currentTool == Tool::Scale) {
            glm::vec2 halfWindowSize = glm::vec2(windowSize) * 0.5f;

            glm::vec4 ndcObjPosPreDivide = cam.getProjectionMatrix((float)windowSize.x / windowSize.y) * cam.getViewMatrix() * glm::vec4(selectedTransform.position, 1.0f);
            glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
            ndcObjectPosition /= ndcObjPosPreDivide.w;
            ndcObjectPosition *= 0.5f;
            ndcObjectPosition += 0.5f;
            ndcObjectPosition *= windowSize;
            ndcObjectPosition.y = windowSize.y - ndcObjectPosition.y;

            glm::vec2 ndcMousePos = inputManager.getMousePosition();

            if (startingMouseDistance == -1.0f) {
                startingMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);
            }

            glm::vec2 circlePos = ndcObjectPosition;
            //circlePos.y = windowSize.y - circlePos.y;
            ImGui::GetForegroundDrawList()->AddCircle(ImVec2(circlePos.x, circlePos.y), startingMouseDistance, ImColor(1.0f, 1.0f, 1.0f), getCircleSegments(startingMouseDistance));
            //ImGui::GetForegroundDrawList()->AddCircle(ImVec2(circlePos.x, circlePos.y), 50.0f, ImColor(1.0f, 1.0f, 1.0f));

            float currentMouseDistance = glm::distance(ndcObjectPosition, ndcMousePos);

            glm::vec2 mouseDir = glm::normalize(ndcMousePos - circlePos);
            glm::vec2 lineStart = circlePos + (mouseDir * startingMouseDistance);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(lineStart.x, lineStart.y), ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f));


            float scaleFac = (currentMouseDistance - startingMouseDistance) * 0.01f;

            ImGui::Text("Scale fac: %f", scaleFac);

            if (scaleFac < 0.0f) {
                scaleFac = (glm::pow(1.0f / (-scaleFac + 1.0f), 3.0f)) - 1.0f;
            }

            ImGui::Text("Corrected scale fa: %f", scaleFac);

            if (inputManager.keyHeld(SDL_SCANCODE_LCTRL)) {
                scaleFac = glm::round(scaleFac * 10.0f) / 10.0f;
            }

            selectedTransform.scale = originalObjectTransform.scale + filterAxes(originalObjectTransform.scale * scaleFac, currentAxisLock);

            std::string scaleStr = "Scale: " + std::to_string(selectedTransform.scale.x);

            ImGui::GetForegroundDrawList()->AddText(ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f), scaleStr.c_str());
            ImGui::Text("Starting mouse distance: %f", startingMouseDistance);
            ImGui::Text("Current mouse distance: %f", currentMouseDistance);
            ImGui::Text("Scale: %f", selectedTransform.scale.x);
        }
    }
}