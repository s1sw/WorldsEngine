using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public static class AddComponentPopup
    {
        const string Name = "Add Component";

        private static string lastSearchString = null;

        public static void Show()
        {
            ImGui.OpenPopup(Name);
        }

        public static void Update()
        {
            if (ImGui.BeginPopup(Name))
            {
                foreach (ComponentMetadata meta in MetadataManager.Metadata)
                {
                    if (ImGui.Button(meta.EditorName) && !meta.ExistsOn(Editor.CurrentlySelected))
                    {
                        meta.Create(Editor.CurrentlySelected);
                        ImGui.CloseCurrentPopup();
                    }
                }

                ImGui.EndPopup();
            }
        }
    }
}
