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
using WorldsEngine.Hotloading;
using WorldsEngine.Input;

namespace WorldsEngine
{
    internal class Engine
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
            
            if (libraryName == "PHYSFS")
            {
                // When using a debug build, the DLL is named "physfsd.dll", whereas in a release build it's named
                // "physfs.dll". DllImports must have constant arguments, so to deal with this we intercept
                // the DLL loading call and redirect it to the correct place.
                if (_isDebug)
                {
                    return GetModuleHandleA("physfsd.dll");
                }
                else
                {
                    return GetModuleHandleA("physfs.dll");
                }
            }

            return IntPtr.Zero;
        }
#endif

        internal static AssemblyLoadManager AssemblyLoadManager = new();
        static readonly EngineSynchronizationContext updateSyncContext = new();
        static readonly EngineSynchronizationContext simulateSyncContext = new();
        static readonly EngineSynchronizationContext editorUpdateSyncContext = new();

        static double _simulationTime = 0.0;
        static double _updateTime = 0.0;
        static bool _isDebug = false;
        static bool _editorActive = false;

        public static bool InEditor => _editorActive;

        public static bool SceneRunning { get; private set; }

        static void ActualInit(IntPtr registryPtr, bool editorActive)
        {
            NativeLibrary.SetDllImportResolver(typeof(Engine).Assembly, ImportResolver);
            Registry.nativeRegistryPtr = registryPtr;

            MetadataManager.Initialise();
            Console.Initialise();

            if (!editorActive)
                AssemblyLoadManager.RegisterAssembly(Path.GetDirectoryName(System.Diagnostics.Process.GetCurrentProcess().MainModule!.FileName) + @"\GameAssemblies\Game.dll");
            Log.Msg($".NET initialised, isDebug: {_isDebug}");
            _editorActive = editorActive;
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static bool Init(IntPtr registryPtr, bool editorActive, bool isDebug)
        {
            _isDebug = isDebug;
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
            Physics.ContactModCallback = null;
            Logger.Log("Scene started!");

            simulateSyncContext.ClearCallbacks();
            updateSyncContext.ClearCallbacks();

            SceneRunning = true;
            if (_editorActive)
                Editor.Editor.Notify("Scene started");

            Registry.OverrideTransformToDPAPose = true;

            SynchronizationContext.SetSynchronizationContext(simulateSyncContext);

            foreach (var system in AssemblyLoadManager.Systems)
            {
                try
                {
                    system.OnSceneStart();
                }
                catch (Exception e)
                {
                    Log.Error($"Error starting system {system.GetType().FullName}: {e}");
                }
            }

            Registry.OnSceneStart();
            Registry.OverrideTransformToDPAPose = false;
        }

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++")]
        static void Update(float deltaTime, float interpAlpha)
        {
            SynchronizationContext.SetSynchronizationContext(updateSyncContext);
            Time.DeltaTime = deltaTime;
            Time.InterpolationAlpha = interpAlpha;
            Time.CurrentTime = _updateTime;

            AssemblyLoadManager.ReloadIfNecessary();

            try
            {
                updateSyncContext.RunCallbacks();

                foreach (ISystem system in AssemblyLoadManager.Systems)
                {
                    system.OnUpdate();
                }

                Registry.RunUpdateOnComponents();
                Awaitables.NextFrame.Wrapped.Run();
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

                foreach (ISystem system in AssemblyLoadManager.Systems)
                {
                    system.OnSimulate();
                }

                Registry.RunSimulateOnComponents();
                Awaitables.NextSimulationTick.Wrapped.Run();
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
            SynchronizationContext.SetSynchronizationContext(editorUpdateSyncContext);
            try
            {
                AssemblyLoadManager.ReloadIfNecessary();
                if (Editor.Editor.State != GameState.Playing)
                    Physics.ClearCollisionQueue();

                Editor.Editor.Update();

                editorUpdateSyncContext.RunCallbacks();
                Registry.ClearDestroyQueue();
            }
            catch (Exception e)
            {
                // There should never be exceptions this far up in editor code.
                // Catch and rethrow so we can redirect it to our logs.
                Log.Error($"Caught exception {e}");
                throw;
            }
            SceneRunning = false;
        }
    }
}
