using System;
using System.Collections.Generic;
using System.Runtime.Loader;
using System.Reflection;

namespace WorldsEngine
{
    internal class GameAssemblyManager
    {
        public IReadOnlyList<ISystem> Systems => gameSystems;

        AssemblyLoadContext loadContext;
        public Assembly assembly { get; private set; }
        readonly List<ISystem> gameSystems = new List<ISystem>();
        readonly List<SerializedType> serializedSystems = new List<SerializedType>();

        private void LoadAssemblySystems(Registry registry)
        {
            foreach (Type systemType in assembly.GetTypes())
            {
                if (!typeof(ISystem).IsAssignableFrom(systemType))
                {
                    continue;
                }

                gameSystems.Add((ISystem)Activator.CreateInstance(systemType, registry));
            }

            Logger.Log($"Loaded {gameSystems.Count} sytems");
        }

        private void LoadSerializedSystems(Registry registry)
        {
            foreach (var serializedSystem in serializedSystems)
            {
                gameSystems.Add((ISystem)HotloadSerialization.Deserialize(assembly, serializedSystem, new object[] { registry }));
            }

            serializedSystems.Clear();

            Logger.Log($"Deserialized {gameSystems.Count}");
        }

        private void SerializeSystems()
        {
            foreach (var system in gameSystems)
            {
                serializedSystems.Add(HotloadSerialization.Serialize(system));
            }
        }

        public void LoadGameAssembly(Registry registry)
        {
            Logger.Log("Loading DLL...");
            loadContext = new AssemblyLoadContext("Game Context", true);
            Logger.Log("AssemblyLoadContext created");
            assembly = loadContext.LoadFromAssemblyPath(System.IO.Path.GetFullPath("GameAssemblies/Game.dll"));

            if (assembly == null)
            {
                Logger.LogError("Failed to load game assembly!");
                return;
            }

            Logger.Log("Loaded assembly");

            if (serializedSystems.Count == 0)
            {
                LoadAssemblySystems(registry);
            }
            else
            {
                LoadSerializedSystems(registry);
            }
        }

        public void UnloadGameAssembly()
        {
            Logger.Log("Serializing systems...");
            SerializeSystems();

            Logger.Log("Unloading DLL...");

            assembly = null;
            gameSystems.Clear();
            loadContext.Unload();

            WeakReference weakRef = new WeakReference(loadContext, trackResurrection: true);
            loadContext = null;
            for (int i = 0; weakRef.IsAlive && (i < 20); i++)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            if (weakRef.IsAlive)
                Logger.LogWarning("Failed to fully unload assembly after 20 iterations");
        }

        public void ReloadGameAssembly(Registry registry)
        {
            UnloadGameAssembly();
            LoadGameAssembly(registry);
        }
    }
}
