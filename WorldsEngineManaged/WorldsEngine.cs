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
        internal const string NATIVE_MODULE = "lonelygalaxy";
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
            if (libraryName == NATIVE_MODULE)
                handle = dlopen(null, RTLD_NOW | RTLD_NOLOAD);

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
                if (ImGui.Begin("Hello from .NET!"))
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

                    if (ImGui.Button("Set a thing"))
                    {
                        Entity entity = new Entity(0);
                        CustomComponent c = new CustomComponent();
                        c.whatever = 1.0f;
                        c.lol = 1337;
                        registry.SetComponent<CustomComponent>(entity, c);
                    }

                    if (ImGui.Button("Get a thing"))
                    {
                        Entity entity = new Entity(0);
                        var comp = registry.GetComponent<CustomComponent>(entity);
                        Logger.Log($"lol: {comp.lol}");
                    }

                    ImGui.Text("Entities:");

                    registry.Each((Entity entity) => {
                        Transform t = registry.GetTransform(entity);
                        string name = registry.HasName(entity) ? registry.GetName(entity) : entity.ID.ToString();
                        ImGui.Text($"{name}: {t.position.x:0.###}, {t.position.y:0.###}, {t.position.z:0.###}");
                    });

                    ImGui.End();
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
                ImGui.End();
            }
        }
    }
}
