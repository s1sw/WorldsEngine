#include "NodeEditor.hpp"
#include <Editor/GuiUtil.hpp>
#include <imgui/imgui.h>

namespace worlds::nodes
{
    Node* currentNode;

    NodeEditor::NodeEditor() : scrollOffset(0.0f)
    {
        DataType* dt = new DataType;
        dt->color = static_cast<ImU32>(ImColor(255, 0, 0));
        dt->name = "Test Datatype";

        NodeType* nt = new NodeType();
        nt->name = "Test Node #1";
        nt->inPorts.add(Port{"Test Port", dt});
        nt->inPorts.add(Port{"Test Port", dt});
        nt->inPorts.add(Port{"Test Port", dt});

        Node* n = new Node();
        n->type = nt;
        n->position = glm::vec2(0.0f);
        n->size = glm::vec2(300, 400);
        currentNode = n;
    }

    void drawNode(Node* node, glm::vec2 offset)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        glm::vec2 position = node->position + offset;
        glm::vec2 size = node->size;

        ImGui::SetCursorScreenPos(position);

        dl->AddRectFilled(position, position + size, ImGui::GetColorU32(ImGuiCol_TitleBg), 5.0f);
        dl->AddRect(position, position + size, ImGui::GetColorU32(ImGuiCol_Border), 5.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
        if (ImGui::BeginChild("node", size))
        {
            pushBoldFont();
            glm::vec2 ts = ImGui::CalcTextSize(node->type->name.c_str());

            dl->AddRectFilled(ImGui::GetCursorScreenPos(),
                              static_cast<glm::vec2>(ImGui::GetCursorScreenPos()) + glm::vec2(size.x, ts.y),
                              ImGui::GetColorU32(ImGuiCol_Header), 5.0f);

            ImGui::Indent(5.0f);
            ImGui::Text(node->type->name.c_str());

            ImGui::PopFont();

            ImGui::Indent(5.0f);
            for (const Port& p : node->type->inPorts)
            {
                glm::vec2 circlePos = ImGui::GetCursorScreenPos();
                circlePos.y += ImGui::GetTextLineHeight() * 0.5f;

                dl->AddCircle(circlePos, 5.0f, p.type->color);

                ImGui::Indent(8.0f);
                ImGui::Text("%s", p.name.c_str());
                ImGui::Indent(-8.0f);
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
            {
                node->position += static_cast<glm::vec2>(ImGui::GetIO().MouseDelta);
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    void NodeEditor::draw()
    {
        glm::vec2 size = ImGui::GetContentRegionAvail();

        if (ImGui::BeginChild("Nodes", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            glm::vec2 offset = ImGui::GetCursorScreenPos();

            if (ImGui::IsMousePosValid() && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            {
                scrollOffset += static_cast<glm::vec2>(ImGui::GetIO().MouseDelta);
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();

            const float gridSpacing = 2.0f * ImGui::GetTextLineHeight();

            glm::vec2 gridOffset{fmodf(scrollOffset.x, gridSpacing), fmodf(scrollOffset.y, gridSpacing)};
            gridOffset += offset;

            for (float x = 0.0f; x < size.x; x += gridSpacing)
            {
                dl->AddLine(gridOffset + glm::vec2(x, -size.y), gridOffset + glm::vec2(x, size.y * 2.0f),
                            ImColor(1.f, 1.f, 1.f, 0.1f));
            }

            for (float y = 0.0f; y < size.y; y += gridSpacing)
            {
                dl->AddLine(gridOffset + glm::vec2(-size.x, y), gridOffset + glm::vec2(size.x * 2.0f, y),
                            ImColor(1.f, 1.f, 1.f, 0.1f));
            }

            // draw node
            drawNode(currentNode, offset + scrollOffset);
        }
        ImGui::EndChild();
    }
}