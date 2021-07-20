using System;
using System.Collections.Generic;
using System.Runtime.Loader;
using System.Reflection;

namespace WorldsEngine
{
    struct SerializedField
    {
        public string FieldName;
        public object Value;
        public string FullTypeName;
        public Type Type;
    }

    struct SerializedType
    {
        public string FullName;
        public Dictionary<string, SerializedField> Fields;
    };

    internal class GameAssemblyManager
    {
        const BindingFlags SerializedFieldBindingFlags =
              BindingFlags.NonPublic
            | BindingFlags.Public
            | BindingFlags.FlattenHierarchy
            | BindingFlags.Instance;

        public IReadOnlyList<ISystem> Systems => gameSystems;

        AssemblyLoadContext loadContext;
        Assembly assembly;
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
                var systemType = assembly.GetType(serializedSystem.FullName, true);
                var system = Activator.CreateInstance(systemType, registry);

                var fields = systemType.GetFields(SerializedFieldBindingFlags);
                foreach (var field in fields)
                {
                    if (serializedSystem.Fields.ContainsKey(field.Name))
                        field.SetValue(system, serializedSystem.Fields[field.Name].Value);
                }

                gameSystems.Add((ISystem)system);
            }

            serializedSystems.Clear();

            Logger.Log($"Deserialized {gameSystems.Count}");
        }

        private void SerializeSystems()
        {
            foreach (var system in gameSystems)
            {
                var systemType = system.GetType();
                var serializedSystem = new SerializedType();
                serializedSystem.FullName = systemType.FullName;
                serializedSystem.Fields = new Dictionary<string, SerializedField>();

                var fields = systemType.GetFields(SerializedFieldBindingFlags);
                foreach (var field in fields)
                {
                    var serializedField = new SerializedField();
                    serializedField.FieldName = field.Name;
                    serializedField.Type = field.FieldType;
                    serializedField.Value = field.GetValue(system);

                    serializedSystem.Fields.Add(field.Name, serializedField);
                }

                serializedSystems.Add(serializedSystem);
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
