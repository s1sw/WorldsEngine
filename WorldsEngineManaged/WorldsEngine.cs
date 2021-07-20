using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.IO;

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

        static Registry registry;
        static FileSystemWatcher gameDllWatcher;
        static DateTime lastReloadTime;
        static GameAssemblyManager assemblyManager;

        static void ActualInit(IntPtr registryPtr)
        {
#if Linux
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
#endif
            registry = new Registry(registryPtr);
            assemblyManager = new GameAssemblyManager();
            assemblyManager.LoadGameAssembly(registry);

            gameDllWatcher = new FileSystemWatcher(System.IO.Path.GetFullPath("GameAssemblies"));

            gameDllWatcher.Filter = "";
            gameDllWatcher.NotifyFilter = NotifyFilters.Attributes
                                 | NotifyFilters.CreationTime
                                 | NotifyFilters.LastAccess
                                 | NotifyFilters.LastWrite
                                 | NotifyFilters.Size;

            gameDllWatcher.IncludeSubdirectories = true;
            gameDllWatcher.Changed += OnDLLChanged;
            gameDllWatcher.Renamed += OnDLLChanged;
            gameDllWatcher.EnableRaisingEvents = true;
        }

        static void OnDLLChanged(object sender, FileSystemEventArgs e)
        {
            if ((DateTime.Now - lastReloadTime).Milliseconds < 500)
                return;
            lastReloadTime = DateTime.Now;
            Logger.Log("DLL changed, reloading...");
            assemblyManager.ReloadGameAssembly(registry);
        }

        static bool Init(IntPtr registryPtr)
        {
            try
            {
                ActualInit(registryPtr);
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

        static void Update(float deltaTime)
        {
            try
            {
                foreach (var system in assemblyManager.Systems)
                {
                    system.OnUpdate(deltaTime);
                }
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }

        static void EditorUpdate()
        {
            registry.ShowDebugWindow();
            if (ImGui.Begin("Hello :)"))
            {
                ImGui.Text("hi");
                if (ImGui.Button("Destroy Those Dang Arrows"))
                {
                    registry.Each((Entity entity) => {
                        Transform t = registry.GetTransform(entity);

                        if (t.position.y < -9000.0f)
                        {
                            registry.Destroy(entity);
                        }
                    });
                }

                if (ImGui.Button("Reload DLL"))
                {
                    assemblyManager.ReloadGameAssembly(registry);
                }
                ImGui.End();
            }
        }
    }
}
