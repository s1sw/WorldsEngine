#include "Editor.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include <glm/gtx/norm.hpp>
#include "ImGuizmo.h"
#include "2DClip.hpp"
#include <glm/gtc/type_ptr.hpp>
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
    , currentAxisLock(AxisFlagBits::All)
    , lookX(0.0f)
    , lookY(0.0f)
    , settings() {
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

void Editor::saveScene(AssetID id) {

}

void Editor::activateTool(Tool newTool) {
    assert(reg.valid(currentSelectedEntity));
    currentTool = newTool;
    originalObjectTransform = reg.get<Transform>(currentSelectedEntity);
    currentAxisLock = AxisFlagBits::All;
    startingMouseDistance = -1.0f;
}

void Editor::update(float deltaTime) {
    if (currentTool == Tool::None && reg.valid(currentSelectedEntity)) {
        // Right mouse button means that the view's being moved, so we'll need the movement keys
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
            glm::vec3 newPos;

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
            
            glm::mat4 vp = cam.getProjectionMatrix((float)windowSize.x / windowSize.y)* cam.getViewMatrix();

            if (getNumActiveAxes(currentAxisLock) == 2) {
                ImGui::GetBackgroundDrawList()->AddLine(worldToScreen(glm::vec3(0.0f), vp), worldToScreen(n, vp), ImColor(1.0f, 1.0f, 1.0f), 2.0f);
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

                ImGui::GetBackgroundDrawList()->AddLine(glmToImgui(startScreen), glmToImgui(endScreen), ImColor(color.x, color.y, color.z), 2.0f);
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
            ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(circlePos.x, circlePos.y), startingMouseDistance, ImColor(1.0f, 1.0f, 1.0f), getCircleSegments(startingMouseDistance));

            // Line from mouse to scale circle
            glm::vec2 mouseDir = glm::normalize(ndcMousePos - circlePos);
            glm::vec2 lineStart = circlePos + (mouseDir * startingMouseDistance);
            ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lineStart.x, lineStart.y), ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f));

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

            ImGui::GetForegroundDrawList()->AddText(ImVec2(ndcMousePos.x, ndcMousePos.y), ImColor(1.0f, 1.0f, 1.0f), scaleStr.c_str());
        }
    }

    updateCamera(deltaTime);

    if (reg.valid(currentSelectedEntity)) {
        if (inputManager.keyHeld(SDL_SCANCODE_LSHIFT) && inputManager.keyPressed(SDL_SCANCODE_D)) {
            auto newEnt = reg.create();

            reg.emplace<Transform>(newEnt, reg.get<Transform>(currentSelectedEntity));
            reg.emplace<WorldObject>(newEnt, reg.get<WorldObject>(currentSelectedEntity));
            select(newEnt);
            currentTool = Tool::Translate;
        }

        if (ImGui::Begin("Selected entity")) {
            if (reg.has<Transform>(currentSelectedEntity)) {
                auto& selectedTransform = reg.get<Transform>(currentSelectedEntity);
                ImGui::DragFloat3("Position", &selectedTransform.position.x);

                glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(selectedTransform.rotation));
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot))) {
                    selectedTransform.rotation = glm::radians(eulerRot);
                }

                ImGui::DragFloat3("Scale", &selectedTransform.scale.x);
            }

            if (reg.has<WorldObject>(currentSelectedEntity)) {
                auto& worldObject = reg.get<WorldObject>(currentSelectedEntity);
                ImGui::DragFloat2("Texture Scale", &worldObject.texScaleOffset.x);
                ImGui::DragFloat2("Texture Offset", &worldObject.texScaleOffset.z);
            }
        }

        ImGui::End();

        if (inputManager.keyPressed(SDL_SCANCODE_DELETE)) {
            reg.destroy(currentSelectedEntity);
        }
    }
}