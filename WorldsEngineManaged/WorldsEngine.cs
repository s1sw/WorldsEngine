using System;
using System.Runtime.Loader;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.InteropServices;

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
        static AssemblyLoadContext loadContext;
        static Assembly gameAssembly = null;
        static List<ISystem> gameSystems = new List<ISystem>();

        static bool Init(IntPtr registryPtr)
        {
#if Linux
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
#endif
            registry = new Registry(registryPtr);
            LoadGameDLL();
            return true;
        }

        static void LoadGameDLL()
        {
            loadContext = new AssemblyLoadContext("Game Context", true);
            gameAssembly = loadContext.LoadFromAssemblyPath(System.IO.Path.GetFullPath("GameAssemblies/Game.dll"));

            if (gameAssembly == null)
            {
                Logger.LogError("Failed to load game assembly!");
                return;
            }

            foreach (Type systemType in gameAssembly.GetTypes())
            {
                if (!typeof(ISystem).IsAssignableFrom(systemType))
                {
                    continue;
                }

                gameSystems.Add((ISystem)Activator.CreateInstance(systemType, registry));
            }

            Logger.Log($"Loaded {gameSystems.Count} sytems");
        }

        static void UnloadDLL()
        {
            loadContext.Unload();
            gameSystems.Clear();
        }

        static void OnSceneStart()
        {
            Logger.Log("Scene started!");

            foreach (var system in gameSystems)
            {
                system.OnSceneStart();
            }
        }

        static void Update(float deltaTime)
        {
            try
            {
                foreach (var system in gameSystems)
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
            if (ImGui.Begin("Hello :)")) {
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
                    UnloadDLL();
                    LoadGameDLL();
                }
                ImGui.End();
            }
        }
    }
}
