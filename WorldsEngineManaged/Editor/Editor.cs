using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using ImGuiNET;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Hotloading;
using WorldsEngine.ComponentMeta;
using WorldsEngine.ECS;
using System.IO;
using System.Collections;

namespace WorldsEngine.Editor
{
    public enum NotificationType : int
    {
        Info,
        Warning,
        Error
    }

    public enum GameState : byte
    {
        Editing,
        Playing,
        Paused
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SlibList
    {
        public IntPtr Data;
        public ulong NumElements;
    }

    public unsafe class NativeList<T> where T : unmanaged
    {
        private SlibList* nativeList;

        public int Count => (int)nativeList->NumElements; 

        public T this[int index] { get => ((T*)nativeList->Data)[index]; }

        internal NativeList(IntPtr nativePtr)
        {
            nativeList = (SlibList*)nativePtr;
        }
    }

    public static class Editor
    {
        #region Native Imports
        [DllImport(Engine.NativeModule)]
        private static extern uint editor_getCurrentlySelected();

        [DllImport(Engine.NativeModule)]
        private static extern void editor_select(uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void editor_addNotification(string notification, NotificationType type);

        [DllImport(Engine.NativeModule)]
        private static extern void editor_overrideHandle(uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern GameState editor_getCurrentState();

        [DllImport(Engine.NativeModule)]
        private static extern IntPtr editor_getSelectionList();
        #endregion

        public static Entity CurrentlySelected => new(editor_getCurrentlySelected());
        public static GameState State => editor_getCurrentState();
        public static NativeList<Entity> SelectedEntities;

        private readonly static Dictionary<Type, EditorWindow> _singleInstanceWindows = new();
        private readonly static List<EditorWindow> _editorWindows = new();

        private readonly static List<Type> _editorWindowTypes = new();

        static Editor()
        {
            _editorWindowTypes = Assembly.GetExecutingAssembly().GetTypes()
                .Where(t => t.IsSubclassOf(typeof(EditorWindow))).ToList();
            
            SelectedEntities = new NativeList<Entity>(editor_getSelectionList());
        }

        public static void Select(Entity entity)
        {
            editor_select(entity.ID);
        }

        public static void Notify(string content, NotificationType type = NotificationType.Info)
        {
            if (!Engine.InEditor) return;

            editor_addNotification(content, type);
        }

        public static void OverrideHandle(Entity entity) => editor_overrideHandle(entity.ID);

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

        private static void ConvertLong(long val, Span<char> outBuffer)
        {
            int index = 0;
            int numNums = (int)(System.Math.Floor(System.Math.Log10(val))) + 1;

            while (val > 0)
            {
                outBuffer[numNums - index - 1] = (char)('0' + (char)(val % 10));
                val /= 10;
                index++;
            }
        }

        private static int GetRequiredNumberBufferLength(long val)
        {
            return (int)(System.Math.Floor(System.Math.Log10(val))) + 1;
        }

        private static bool _miscWindowOpen = false;
        internal static void DrawMiscWindow()
        {
            if (!_miscWindowOpen) return;
            if (ImGui.Begin("Misc", ref _miscWindowOpen))
            {
                // Do some very janky string stuff so we don't allocate any memory
                ImGui.TextUnformatted("Current managed memory usage: ");
                ImGui.SameLine();

                long memUsage = GC.GetTotalMemory(false);
                var len = GetRequiredNumberBufferLength(memUsage / 1000);

                Span<char> buffer = stackalloc char[len + 1]; // 1 more for letter K
                ConvertLong(memUsage / 1000, buffer);
                buffer[len] = 'K';

                int byteCount = System.Text.Encoding.UTF8.GetByteCount(buffer);

                Span<byte> nativeTextBuffer = stackalloc byte[byteCount + 1];
                System.Text.Encoding.UTF8.GetBytes(buffer, nativeTextBuffer);
                nativeTextBuffer[byteCount] = 0;

                unsafe
                {
                    fixed (byte* nativeText = nativeTextBuffer)
                    {
                        ImGuiNative.igTextUnformatted(nativeText, null);
                    }
                }

                if (ImGui.Button("Force Collection"))
                {
                    GC.Collect(99, GCCollectionMode.Forced, true, true);
                }

                if (ImGui.Button("Force Reload Assembly"))
                {
                    Engine.AssemblyLoadManager.ReloadAll();
                }

                if (ImGui.Button("Destroy Far-Away Objects"))
                {
                    Registry.Each((Entity e) =>
                    {
                        Transform t = Registry.GetTransform(e);
                        if (t.Position.y < -9000.0f)
                            Registry.Destroy(e);
                    });
                }

                if (ImGui.Button("Mark all static PhysicsActors as static WorldObjects"))
                {
                    Registry.Each((Entity e) =>
                    {
                        if (Registry.HasComponent<PhysicsActor>(e) && Registry.HasComponent<WorldObject>(e))
                        {
                            var wo = Registry.GetComponent<WorldObject>(e);
                            wo.StaticFlags = StaticFlags.Audio | StaticFlags.Rendering | StaticFlags.Navigation;
                        }
                    });
                }
            }
            ImGui.End();
        }

        internal static void Update()
        {
            if (ImGui.BeginMainMenuBar())
            {
                if (ImGui.BeginMenu("Window"))
                {
                    ImGui.Separator();
                    if (ImGui.MenuItem("Misc"))
                    {
                        _miscWindowOpen = true;
                    }

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
            DrawMiscWindow();

            foreach (Type t in Registry.GetTypesWithAttribute(typeof(GizmoAttribute)))
            {
                GizmoAttribute ga = t.GetCustomAttribute<GizmoAttribute>()!;

                foreach (var entity in Registry.View(t))
                {
                    GizmoRenderer.DrawGizmo(entity, ga.GizmoPath);
                }
            }

            if (Registry.Valid(CurrentlySelected))
            {
                MetadataManager.DrawGizmosFor(CurrentlySelected);
            }

            for (int i = 0; i < SelectedEntities.Count; i++)
            {
                if (Registry.Valid(SelectedEntities[i]))
                {
                    MetadataManager.DrawGizmosFor(SelectedEntities[i]);
                }
            }
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

        private static string? lastAssemblyPath;

        private static void OnGameProjectSelected(IntPtr nativeProject)
        {
            GameProject gp = new(nativeProject);

            Log.Msg($"project selected: {gp.Name}");
            Log.Msg($"root: {gp.Root}");

            lastAssemblyPath = gp.Root + "/CompiledCode/Game.dll";

            Log.Msg($"Loading assembly {lastAssemblyPath}");

            Engine.AssemblyLoadManager.RegisterAssembly(lastAssemblyPath);
        }

        private static void OnGameProjectClosed()
        {
            if (lastAssemblyPath == null) return;

            Engine.AssemblyLoadManager.UnregisterAssembly(lastAssemblyPath);
            lastAssemblyPath = null;
        }
    }
}
