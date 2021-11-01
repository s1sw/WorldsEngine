using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using ImGuiNET;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Editor
{
    public enum NotificationType : int
    {
        Info,
        Warning,
        Error
    }

    public static class Editor
    {
        #region Native Imports
        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint editor_getCurrentlySelected();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void editor_select(uint entity);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void editor_addNotification(string notification, NotificationType type);
        #endregion

        public static Entity CurrentlySelected => new Entity(editor_getCurrentlySelected());

        private readonly static Dictionary<Type, EditorWindow> _singleInstanceWindows = new();
        private readonly static List<EditorWindow> _editorWindows = new();

        private readonly static List<Type> _editorWindowTypes = new();

        static Editor()
        {
            _editorWindowTypes = Assembly.GetExecutingAssembly().GetTypes()
                .Where(t => t.IsSubclassOf(typeof(EditorWindow))).ToList();
        }

        public static void Select(Entity entity)
        {
            editor_select(entity.ID);
        }

        public static void Notify(string content, NotificationType type = NotificationType.Info) => editor_addNotification(content, type);

        public static void OpenWindowOfType(Type type)
        {
            EditorWindowAttribute? windowAttribute = type.GetCustomAttribute<EditorWindowAttribute>();

            if (windowAttribute == null)
                throw new ArgumentException($"{type.FullName} is not an editor window type");
            
            EditorWindow? instance = null;

            // First check if we have an already instantiated closed window of that type...
            foreach (EditorWindow ew in _editorWindows)
            {
                if (!ew.IsOpen && ew.GetType() == type)
                {
                    instance = ew;
                    break;
                }
            }

            // If we don't, just create it.
            if (instance == null)
                instance = CreateWindowOfType(type);

            instance.Open();
        }

        internal static void Update()
        {
            if (ImGui.BeginMainMenuBar())
            {
                if (ImGui.BeginMenu("Window"))
                {
                    ImGui.Separator();
                    
                    foreach (Type t in _editorWindowTypes)
                    {
                        if (ImGui.MenuItem(t.Name))
                            OpenWindowOfType(t);
                    }

                    ImGui.EndMenu();
                }

                ImGui.EndMainMenuBar();
            }

            DrawOpenWindows();
        }

        private static EditorWindow CreateWindowOfType(Type t)
        {
            bool allowMultipleInstances = t.GetCustomAttribute<EditorWindowAttribute>()!.AllowMultipleInstances;

            if (!allowMultipleInstances && _singleInstanceWindows.ContainsKey(t))
                return _singleInstanceWindows[t];

            EditorWindow ew = (Activator.CreateInstance(t) as EditorWindow)!;

            if (!allowMultipleInstances)
                _singleInstanceWindows.Add(t, ew);

            _editorWindows.Add(ew);

            return ew;
        }

        private static void DrawOpenWindows()
        {
            foreach (EditorWindow window in _editorWindows)
            {
                if (window.IsOpen)
                    window.Draw();
            }
        }
    }
}
