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
using WorldsEngine.Editor;

namespace WorldsEngine
{
    internal class WorldsEngine
    {
        internal const string NativeModule = "WorldsEngineNative";

#if Linux
        const int RTLD_NOW = 0x00002;
        const int RTLD_NOLOAD = 0x00004;
        [DllImport("libdl.so.2")]
        internal static extern IntPtr dlopen(string? file, int mode);

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

        internal static HotloadManager HotloadManager = new();
        static readonly EngineSynchronizationContext updateSyncContext = new();
        static readonly EngineSynchronizationContext simulateSyncContext = new();
        static readonly EngineSynchronizationContext editorUpdateSyncContext = new();

        static double _simulationTime = 0.0;
        static double _updateTime = 0.0;

        public static bool SceneRunning { get; private set; }

        static void ActualInit(IntPtr registryPtr, bool editorActive)
        {
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
            Registry.nativeRegistryPtr = registryPtr;

            // These depend on game assembly metadata so
            // initialise them explicitly before loading the assembly.
            MetadataManager.Initialise();
            Console.Initialise();

            HotloadManager.Active = editorActive;

            GameAssemblyManager.OnAssemblyUnload += () =>
            {
                updateSyncContext.ClearCallbacks();
                simulateSyncContext.ClearCallbacks();
            };

            GameAssemblyManager.OnAssemblyLoad += (Assembly) =>
            {
                Editor.Editor.Notify("Assembly loaded");
            };
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static bool Init(IntPtr registryPtr, bool editorActive)
        {
            try
            {
                ActualInit(registryPtr, editorActive);
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

            simulateSyncContext.ClearCallbacks();
            updateSyncContext.ClearCallbacks();
            Registry.OverrideTransformToDPAPose = true;

            try
            {
                foreach (var system in HotloadManager.Systems)
                {
                    system.OnSceneStart();
                }

                Registry.OnSceneStart();
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }

            Registry.OverrideTransformToDPAPose = false;
            SceneRunning = true;
            Editor.Editor.Notify("Scene started");
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void Update(float deltaTime)
        {
            HotloadManager.ReloadIfNecessary();
            SynchronizationContext.SetSynchronizationContext(updateSyncContext);
            Time.DeltaTime = deltaTime;
            Time.CurrentTime = _updateTime;

            try
            {
                updateSyncContext.RunCallbacks();

                for (int i = 0; i < HotloadManager.Systems.Count; i++)
                {
                    HotloadManager.Systems[i].OnUpdate();
                }

                Registry.RunUpdateOnComponents();
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }

            Registry.ClearDestroyQueue();

            _updateTime += Time.DeltaTime;
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void Simulate(float deltaTime)
        {
            SynchronizationContext.SetSynchronizationContext(simulateSyncContext);
            Time.DeltaTime = deltaTime;
            Time.CurrentTime = _simulationTime;

            Registry.OverrideTransformToDPAPose = true;
            try
            {
                Physics.FlushCollisionQueue();
                simulateSyncContext.RunCallbacks();

                for (int i = 0; i < HotloadManager.Systems.Count; i++)
                {
                    HotloadManager.Systems[i].OnSimulate();
                }

                Registry.RunSimulateOnComponents();
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }

            Registry.ClearDestroyQueue();
            Registry.OverrideTransformToDPAPose = false;

            _simulationTime += Time.DeltaTime;
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void EditorUpdate()
        {
            HotloadManager.ReloadIfNecessary();
            SynchronizationContext.SetSynchronizationContext(editorUpdateSyncContext);
            Physics.ClearCollisionQueue();

            Editor.Editor.Update();

            editorUpdateSyncContext.RunCallbacks();
            SceneRunning = false;
        }
    }
}
