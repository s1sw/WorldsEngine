using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    internal class WorldsEngine
    {
#if Windows
        internal const string NATIVE_MODULE = "lonelygalaxy.exe";
#elif Linux
        internal const string NATIVE_MODULE = "internal";
#else
#error Unknown platform
#endif

#if Linux
        [DllImport("dl")]
        internal static extern IntPtr dlopen(string file, int mode);

        private static IntPtr ImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            IntPtr handle = IntPtr.Zero;

            // On Linux, you can't just load an executable as a library like Windows.
            // However, if you pass a null pointer as the filename to dlopen it
            // returns the address of the executable, which you can pass to dlsym.
            // So we just dlopen null and return that.
            if (libraryName == NATIVE_MODULE)
                handle = dlopen(null, 2);

            return handle;
        }
#endif

        static Registry registry;

        static bool Init()
        {
#if Linux
            NativeLibrary.SetDllImportResolver(typeof(WorldsEngine).Assembly, ImportResolver);
#endif
            return true;
        }

        static void OnSceneStart(IntPtr registryPtr)
        {
            Logger.Log("Scene started!");
            registry = new Registry(registryPtr);
        }

        static void Update(float deltaTime)
        {
            try
            {
                using (ImGui.Window("Hello from .NET!"))
                {
                    ImGui.Text("Hey, how's it goin'? :)");

                    if (ImGui.Button("Click me!"))
                    {
                        Logger.Log("Tee-hee");
                    }

                    if (ImGui.Button("Throw exception"))
                    {
                        throw new ApplicationException("I mean, what did you expect?");
                    }

                    if (ImGui.Button("Move a thing"))
                    {
                        Entity entity = new Entity(0);
                        Transform t = registry.GetTransform(entity);
                        t.position = new Vector3(0.0f, 10.0f, 0.0f);
                        registry.SetTransform(entity, t);
                    }

                    ImGui.Text("Entities:");

                    registry.Each((Entity entity) => {
                        Transform t = registry.GetTransform(entity);
                        ImGui.Text($"{entity.ID}: {t.position.x}, {t.position.y}, {t.position.z}");
                    });
                }
            }
            catch (Exception e)
            {
                Logger.LogError($"Caught exception: {e}");
            }
        }
    }
}
