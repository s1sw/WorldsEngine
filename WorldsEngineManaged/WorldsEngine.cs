using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.IO;
using WorldsEngine.ComponentMeta;
using System.Collections.Generic;
using System.Threading;
using ImGuiNET;
using WorldsEngine.Math;

namespace WorldsEngine
{
    internal class WorldsEngine
    {
#if Windows
        internal const string NativeModule = "lonelygalaxy.exe";
#elif Linux
        internal const string NativeModule = "lonelygalaxy";
#else
#error Unknown platform
#endif

#if Linux
        const int RTLD_NOW = 0x00002;
        const int RTLD_NOLOAD = 0x00004;
        [DllImport("dl")]
        internal static extern IntPtr dlopen(string file, int mode);

        private static IntPtr ImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            IntPtr handle = IntPtr.Zero;

            // On Linux, you can't just load an executable as a library and have it all
            // just work. However, if you pass a null pointer as the filename to dlopen it
            // returns the address of the executable, which you can pass to dlsym.
            // So we just dlopen null and return that.
            // This requires linking with "-rdynamic" to export symbols.
            if (libraryName == NativeModule)
                handle = dlopen(null, RTLD_NOW | RTLD_NOLOAD);

            return handle;
        }
#endif

        static FileSystemWatcher gameDllWatcher;
        static DateTime lastReloadTime;
        static GameAssemblyManager assemblyManager;
        static bool reloadAssemblyNextFrame = false;
        static EngineSynchronizationContext updateSyncContext;
        static EngineSynchronizationContext simulateSyncContext;
        static EngineSynchronizationContext editorUpdateSyncContext;

        static void ActualInit(IntPtr registryPtr, IntPtr mainCameraPtr)
        {
#if Linux
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
#endif
            Registry.nativeRegistryPtr = registryPtr; 

            // These depend on game assembly metadata so
            // initialise them explicitly before loading the assembly.
            MetadataManager.Initialise();
            Console.Initialise();

            assemblyManager = new GameAssemblyManager();
            assemblyManager.LoadGameAssembly();

            gameDllWatcher = new FileSystemWatcher(Path.GetFullPath("GameAssemblies"))
            {
                Filter = "",
                NotifyFilter = NotifyFilters.Attributes
                             | NotifyFilters.CreationTime
                             | NotifyFilters.LastAccess
                             | NotifyFilters.LastWrite
                             | NotifyFilters.Size,

                IncludeSubdirectories = true
            };

            gameDllWatcher.Changed += OnDLLChanged;
            gameDllWatcher.Renamed += OnDLLChanged;
            gameDllWatcher.EnableRaisingEvents = true;

            // Clear out all the temp game assembly directories
            foreach (string d in Directory.GetDirectories("."))
            {
                if (d.StartsWith("GameAssembliesTemp"))
                {
                    Directory.Delete(d, true);
                }
            }

            new Camera(mainCameraPtr, true);

            updateSyncContext = new EngineSynchronizationContext();
            simulateSyncContext = new EngineSynchronizationContext();
            editorUpdateSyncContext = new EngineSynchronizationContext();
        }

        static void OnDLLChanged(object sender, FileSystemEventArgs e)
        {
            if ((DateTime.Now - lastReloadTime).TotalMilliseconds < 500)
            {
                Logger.LogWarning("Ignoring assembly reload as too soon");
                return;
            }

            lastReloadTime = DateTime.Now;
            reloadAssemblyNextFrame = true;
            Logger.Log("DLL changed, reloading...");
        }

        static bool Init(IntPtr registryPtr, IntPtr mainCameraPtr)
        {
            try
            {
                ActualInit(registryPtr, mainCameraPtr);
            }
            catch (Exception ex)
            {
                Logger.LogError(ex.ToString());
                return false;
            }

            return true;
        }

        static void OnSceneStart()
        {
            Logger.Log("Scene started!");

            foreach (var system in assemblyManager.Systems)
            {
                system.OnSceneStart();
            }
        }

        static void ReloadAssemblyIfNecessary()
        {
            if (reloadAssemblyNextFrame)
            {
                Registry.SerializeStorages();
                assemblyManager.ReloadGameAssembly();
                reloadAssemblyNextFrame = false;
            }
        }

        static void Update(float deltaTime)
        {
            ReloadAssemblyIfNecessary();
            SynchronizationContext.SetSynchronizationContext(updateSyncContext);
            Time.DeltaTime = deltaTime;

            try
            {
                updateSyncContext.RunCallbacks();

                foreach (var system in assemblyManager.Systems)
                {
                    system.OnUpdate();
                }
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }

        static void Simulate(float deltaTime)
        {
            SynchronizationContext.SetSynchronizationContext(simulateSyncContext);
            Time.DeltaTime = deltaTime;

            try
            {
                simulateSyncContext.RunCallbacks();

                foreach (var system in assemblyManager.Systems)
                {
                    system.OnSimulate();
                }

                Registry.UpdateThinkingComponents();
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }

        static uint PackRGBA(byte r, byte g, byte b, byte a)
        {
            return (uint)(r << 24 | g << 16 | b << 8 | a);
        }

        static void EditorUpdate()
        {
            ReloadAssemblyIfNecessary();
            SynchronizationContext.SetSynchronizationContext(editorUpdateSyncContext);

            if (ImGui.Begin($"{FontAwesome.FontAwesomeIcons.Cube} Selected Entity"))
            {
                if (Editor.CurrentlySelected.IsNull)
                {
                    ImGui.Text("No entity selected");
                }
                else
                {
                    MetadataManager.EditEntity(Editor.CurrentlySelected);

                    if (ImGui.Button("Add Component"))
                    {
                        AddComponentPopup.Show();
                    }
                }

                AddComponentPopup.Update();
            }

            ImGui.End();

            if (ImGui.Begin("Misc"))
            {
                ImGui.Text($"Memory usage: {GC.GetGCMemoryInfo().HeapSizeBytes/1000:N0}K");

                if (ImGui.Button("Force Collection"))
                {
                    GC.Collect(99, GCCollectionMode.Forced, true, true);
                }

                if (ImGui.Button("Force Reload Assembly"))
                {
                    reloadAssemblyNextFrame = true;
                }
            }
            ImGui.End();

            editorUpdateSyncContext.RunCallbacks();
        }
    }
}
