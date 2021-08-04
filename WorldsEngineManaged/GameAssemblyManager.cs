using System;
using System.Collections.Generic;
using System.Runtime.Loader;
using System.Reflection;
using System.IO;
using System.Globalization;

namespace WorldsEngine
{
    internal class GameAssemblyManager
    {
        public IReadOnlyList<ISystem> Systems => gameSystems;

        public Assembly Assembly { get; private set; }

        public static event Action<Assembly> OnAssemblyLoad;
        public static event Action OnAssemblyUnload;

        AssemblyLoadContext loadContext;
        readonly List<ISystem> gameSystems = new List<ISystem>();
        readonly List<SerializedType> serializedSystems = new List<SerializedType>();

        private void CreateAndAddSystem(Type systemType)
        {
            gameSystems.Add((ISystem)Activator.CreateInstance(systemType));
        }

        private void LoadAssemblySystems()
        {
            foreach (Type systemType in Assembly.GetTypes())
            {
                if (!typeof(ISystem).IsAssignableFrom(systemType))
                {
                    continue;
                }

                CreateAndAddSystem(systemType);
            }

            Logger.Log($"Loaded {gameSystems.Count} sytems");
        }

        private void LoadSerializedSystems()
        {
            HashSet<Type> initialisedTypes = new HashSet<Type>();
            foreach (var serializedSystem in serializedSystems)
            {
                ISystem deserializedSystem = (ISystem)HotloadSerialization.Deserialize(serializedSystem);
                gameSystems.Add(deserializedSystem);
                initialisedTypes.Add(deserializedSystem.GetType());
            }

            serializedSystems.Clear();

            Logger.Log($"Deserialized {gameSystems.Count} systems");

            foreach (Type systemType in Assembly.GetTypes())
            {
                if (!typeof(ISystem).IsAssignableFrom(systemType) || initialisedTypes.Contains(systemType))
                {
                    continue;
                }

                CreateAndAddSystem(systemType);
            }
        }

        private void SerializeSystems()
        {
            foreach (var system in gameSystems)
            {
                serializedSystems.Add(HotloadSerialization.Serialize(system));
            }

            gameSystems.Clear();
        }

        private static void DirectoryCopy(string sourceDirName, string destDirName, bool copySubDirs)
        {
            // Get the subdirectories for the specified directory.
            DirectoryInfo dir = new DirectoryInfo(sourceDirName);

            if (!dir.Exists)
            {
                throw new DirectoryNotFoundException(
                    "Source directory does not exist or could not be found: "
                    + sourceDirName);
            }

            DirectoryInfo[] dirs = dir.GetDirectories();

            // If the destination directory doesn't exist, create it.       
            Directory.CreateDirectory(destDirName);

            // Get the files in the directory and copy them to the new location.
            FileInfo[] files = dir.GetFiles();
            foreach (FileInfo file in files)
            {
                string tempPath = Path.Combine(destDirName, file.Name);
                file.CopyTo(tempPath, false);
            }

            // If copying subdirectories, copy them and their contents to new location.
            if (copySubDirs)
            {
                foreach (DirectoryInfo subdir in dirs)
                {
                    string tempPath = Path.Combine(destDirName, subdir.Name);
                    DirectoryCopy(subdir.FullName, tempPath, copySubDirs);
                }
            }
        }

        public void LoadGameAssembly()
        {
            Logger.Log("Loading DLL...");
            loadContext = new AssemblyLoadContext("Game Context", true);

            Logger.Log("AssemblyLoadContext created");

            // On Windows the assembly is locked when it's loaded into memory.
            // To fix this, copy the assembly into a temporary directory and load from there.
            if (Directory.Exists("GameAssembliesTemp"))
            {
                try
                {
                    Directory.Delete("GameAssembliesTemp", true);
                }
                catch (UnauthorizedAccessException)
                {
                    Logger.LogWarning("Failed to delete temp game assemblies directory, renaming instead...");
                    Directory.Move("GameAssembliesTemp", $"GameAssembliesTemp-{DateTime.Now.ToString("yy-MM-dd-HH-mm", DateTimeFormatInfo.InvariantInfo)}");
                }
            }

            DirectoryCopy("GameAssemblies", "GameAssembliesTemp", true);
            Assembly = loadContext.LoadFromAssemblyPath(Path.GetFullPath("GameAssembliesTemp/Game.dll"));

            if (Assembly == null)
            {
                Logger.LogError("Failed to load game assembly!");
                return;
            }

            HotloadSerialization.CurrentGameAssembly = Assembly;

            Logger.Log("Loaded assembly");

            if (serializedSystems.Count == 0)
            {
                LoadAssemblySystems();
            }
            else
            {
                LoadSerializedSystems();
            }

            OnAssemblyLoad?.Invoke(Assembly);
        }

        public void UnloadGameAssembly()
        {
            Logger.Log("Serializing systems...");
            SerializeSystems();

            Logger.Log("Unloading DLL...");

            OnAssemblyUnload?.Invoke();
            Assembly = null;
            HotloadSerialization.CurrentGameAssembly = null;
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

        public void ReloadGameAssembly()
        {
            UnloadGameAssembly();
            LoadGameAssembly();
        }
    }
}
