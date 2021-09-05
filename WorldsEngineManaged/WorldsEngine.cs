using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.IO;
using WorldsEngine.ComponentMeta;
using System.Collections.Generic;
using System.Threading;
using ImGuiNET;
using WorldsEngine.Math;
using JetBrains.Annotations;
using System.Diagnostics.CodeAnalysis;

namespace WorldsEngine
{
    internal class WorldsEngine
    {
        internal const string NativeModule = "WorldsEngineNative";

#if Linux
        const int RTLD_NOW = 0x00002;
        const int RTLD_NOLOAD = 0x00004;
        [DllImport("dl")]
        internal static extern IntPtr dlopen(string file, int mode);

        private static IntPtr ImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            // On Linux, you can't just load an executable as a library and have it all
            // just work. However, if you pass a null pointer as the filename to dlopen it
            // returns the address of the executable, which you can pass to dlsym.
            // So we just dlopen null and return that.
            // This requires linking with "-rdynamic" to export symbols.
            if (libraryName == NativeModule)
                return dlopen(null, RTLD_NOW | RTLD_NOLOAD);

            return IntPtr.Zero;
        }
#elif Windows
        [DllImport("Kernel32")]
        private static extern IntPtr GetModuleHandleA(string? moduleName);

        private static IntPtr ImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (libraryName == NativeModule)
                return GetModuleHandleA(null);

            return IntPtr.Zero;
        }
#endif

        static HotloadManager hotloadManager = new HotloadManager();
        static EngineSynchronizationContext updateSyncContext = new EngineSynchronizationContext();
        static EngineSynchronizationContext simulateSyncContext = new EngineSynchronizationContext();
        static EngineSynchronizationContext editorUpdateSyncContext = new EngineSynchronizationContext();

        static void ActualInit(IntPtr registryPtr, IntPtr mainCameraPtr)
        {
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
            Registry.nativeRegistryPtr = registryPtr;

            // These depend on game assembly metadata so
            // initialise them explicitly before loading the assembly.
            MetadataManager.Initialise();
            Console.Initialise();

            hotloadManager.Active = true;

            Camera.Main = new Camera(mainCameraPtr);

            updateSyncContext = new EngineSynchronizationContext();
            simulateSyncContext = new EngineSynchronizationContext();
            editorUpdateSyncContext = new EngineSynchronizationContext();
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
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

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void OnSceneStart()
        {
            Logger.Log("Scene started!");

            foreach (var system in hotloadManager.Systems)
            {
                system.OnSceneStart();
            }
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void Update(float deltaTime)
        {
            hotloadManager.ReloadIfNecessary();
            SynchronizationContext.SetSynchronizationContext(updateSyncContext);
            Time.DeltaTime = deltaTime;

            try
            {
                updateSyncContext.RunCallbacks();

                foreach (var system in hotloadManager.Systems)
                {
                    system.OnUpdate();
                }
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void Simulate(float deltaTime)
        {
            SynchronizationContext.SetSynchronizationContext(simulateSyncContext);
            Time.DeltaTime = deltaTime;

            try
            {
                simulateSyncContext.RunCallbacks();

                foreach (var system in hotloadManager.Systems)
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

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void EditorUpdate()
        {
            hotloadManager.ReloadIfNecessary();
            SynchronizationContext.SetSynchronizationContext(editorUpdateSyncContext);

            if (ImGui.Begin($"{FontAwesome.FontAwesomeIcons.Cube} Selected Entity"))
            {
                if (Registry.Valid(Editor.CurrentlySelected))
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
                ImGui.Text($"Managed memory usage at last GC: {GC.GetGCMemoryInfo().HeapSizeBytes / 1000:N0}K");

                if (ImGui.Button("Force Collection"))
                {
                    GC.Collect(99, GCCollectionMode.Forced, true, true);
                }

                if (ImGui.Button("Force Reload Assembly"))
                {
                    hotloadManager.ForceReload();
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
            }
            ImGui.End();

            editorUpdateSyncContext.RunCallbacks();
        }
    }
}
