#include "EditorWindows.hpp"
#include <Editor/NodeEditor.hpp>

namespace worlds
{
    static nodes::NodeEditor *ne = nullptr;

    void NodeEditorTest::draw(entt::registry &reg)
    {
        if (!ne)
        {
            ne = new nodes::NodeEditor;
        }

        if (ImGui::Begin("Node Editor Test", &active))
        {
            ne->draw();
        }
        ImGui::End();
    }
}