using ImGuiNET;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.ComponentMeta;
using WorldsEngine.ECS;

namespace WorldsEngine.Editor.EditorWindows
{
    [EditorWindow]
    public class EntityEditor : EditorWindow
    {
        public override void Draw()
        {
            bool open = IsOpen;

            if (ImGui.Begin($"{FontAwesome.FontAwesomeIcons.Cube} Selected Entity", ref open))
            {
                if (!Registry.Valid(Editor.CurrentlySelected))
                {
                    ImGui.Text("No entity selected");
                }
                else
                {
                    ImGui.Text($"ID {Editor.CurrentlySelected.ID}");
                    MetadataManager.EditEntity(Editor.CurrentlySelected);

                    if (ImGui.Button("Add Component"))
                    {
                        AddComponentPopup.Show();
                    }
                }

                AddComponentPopup.Update();
            }

            ImGui.End();

            IsOpen = open;
        }
    }
}
