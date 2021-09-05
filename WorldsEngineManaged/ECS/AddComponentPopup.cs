using ImGuiNET;
using System.Collections.Generic;
using System.Linq;
using WorldsEngine.ComponentMeta;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public static class AddComponentPopup
    {
        const string Name = "Add Component";
        private static string _currentSearchText = string.Empty;
        private static List<ComponentMetadata> _filteredMetadata = new();
        private static bool _justOpened = false;

        public static void Show()
        {
            ImGui.OpenPopup(Name);
            _currentSearchText = string.Empty;
            _filteredMetadata = new List<ComponentMetadata>(MetadataManager.Metadata);
            _justOpened = true;
        }

        public static void Update()
        {
            ImGui.SetNextWindowSizeConstraints(new Vector2(30.0f, 30.0f), new Vector2(float.MaxValue, 400.0f));
            if (ImGui.BeginPopup(Name))
            {
                if (_justOpened)
                {
                    ImGui.SetKeyboardFocusHere();
                    _justOpened = false;
                }

                if (ImGui.InputText("##Search", ref _currentSearchText, 255))
                {
                    _filteredMetadata = MetadataManager.Metadata
                        .Where(m => m.EditorName.ToLower().Contains(_currentSearchText))
                        .ToList();
                }

                foreach (ComponentMetadata meta in _filteredMetadata)
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
