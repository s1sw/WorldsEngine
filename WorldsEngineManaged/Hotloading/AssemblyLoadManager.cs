using System;
using System.Collections.Generic;
using System.Runtime.Loader;
using System.Reflection;
using System.IO;
using System.Linq;
using System.Diagnostics.CodeAnalysis;

namespace WorldsEngine.Hotloading
{
    [System.Serializable]
    internal class AssemblyUnloadFailedException : System.Exception
    {
        public AssemblyUnloadFailedException() { }
        public AssemblyUnloadFailedException(string message) : base(message) { }
        public AssemblyUnloadFailedException(string message, System.Exception inner) : base(message, inner) { }
        protected AssemblyUnloadFailedException(
            System.Runtime.Serialization.SerializationInfo info,
            System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }

    internal class LoadedAssembly
    {
        private static string AssemblyTempDir;

        public readonly string AssemblyPath;
        public Assembly? LoadedAs { get; private set; }
        public AssemblyLoadContext? LoadContext { get; private set; }
        public bool Loaded { get; private set; }
        public bool NeedsReload => needsReload;

        private FileSystemWatcher? watcher;
        private bool needsReload = false;
        private bool currentlyReloading = false;
        private static int loadCounter = 0;

        static LoadedAssembly()
        {
            AssemblyTempDir = Path.Join(Path.GetTempPath(), "WorldsEngineTempAssemblies");

            Directory.CreateDirectory(AssemblyTempDir);

            foreach (string file in Directory.EnumerateFiles(AssemblyTempDir))
            {
                File.Delete(file);
            }
        }

        public LoadedAssembly(string path, bool watch = false)
        {
            AssemblyPath = path;

            if (watch)
            {
                watcher = new FileSystemWatcher(Path.GetDirectoryName(path)!);

                watcher.NotifyFilter = NotifyFilters.Attributes
                             | NotifyFilters.CreationTime
                             | NotifyFilters.LastAccess
                             | NotifyFilters.LastWrite
                             | NotifyFilters.Size;


                watcher.Changed += OnAssemblyChanged;
                watcher.Filter = "*.dll";

                watcher.EnableRaisingEvents = true;
            }
        }

        public void Load()
        {
            if (Loaded)
                throw new InvalidOperationException("Cannot load an assembly that is already loaded.");

            LoadContext = new AssemblyLoadContext(AssemblyPath, true);
            string tempName = CopyTempAssembly();
            LoadedAs = LoadContext.LoadFromAssemblyPath(tempName);

            Log.Msg($"Loaded assembly {AssemblyPath} (really {tempName})");
            Loaded = true;
        }

        public void Unload()
        {
            if (!Loaded)
                throw new InvalidOperationException("Cannot unload an assembly that is not loaded.");

            WeakReference weakRef = ClearLoadContext();
            EnsureAssemblyIsUnloaded(weakRef);

            Log.Msg($"Unloaded assembly {AssemblyPath}");
            Loaded = false;
        }

        public void SwapReload()
        {
            if (!Loaded)
                throw new InvalidOperationException("Cannot reload an assembly that is not loaded.");

            WeakReference weakRef = LoadSwap();
            EnsureAssemblyIsUnloaded(weakRef);
        }

        public void ReloadIfNecessary()
        {
            if (needsReload)
            {
                currentlyReloading = true;
                if (Loaded)
                    SwapReload();
                else
                    Load();
                currentlyReloading = false;

                needsReload = false;
            }
        }

        private WeakReference ClearLoadContext()
        {
            LoadedAs = null;

            LoadContext!.Unload();
            WeakReference weakRef = new(LoadContext);
            LoadContext = null;

            return weakRef;
        }

        private void EnsureAssemblyIsUnloaded(WeakReference loadContext)
        {
            for (int i = 0; loadContext.IsAlive && (i < 20); i++)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            if (loadContext.IsAlive)
            {
                throw new AssemblyUnloadFailedException(
                    $"Failed to unload assembly {AssemblyPath} after 20 iterations! " +
                    "There are likely still live references hiding somewhere."
                );
            }
        }

        // This method is here to encapsulate access to the
        // AssemblyLoadContext and assembly itself. If inside the
        // SwapReload method, the references to the old assembly and load 
        // context on the stack prevent it from unloading properly.
        private WeakReference LoadSwap()
        {
            AssemblyLoadContext newAlc = new(AssemblyPath, true);
            string tempName = CopyTempAssembly();
            Assembly newAssembly = newAlc.LoadFromAssemblyPath(tempName);

            HotloadSwapper swapper = new(LoadedAs!, newAssembly);
            swapper.SwapReferences();

            WeakReference weakRef = ClearLoadContext();

            LoadedAs = newAssembly;
            LoadContext = newAlc;

            return weakRef;
        }

        private string CopyTempAssembly()
        {
            string copyName = Path.Join(AssemblyTempDir, $"{Path.GetFileNameWithoutExtension(AssemblyPath)}-{loadCounter}.dll");
            loadCounter++;

            File.Copy(AssemblyPath, copyName);
            return copyName;
        }

        private void OnAssemblyChanged(object sender, FileSystemEventArgs e)
        {
            if (currentlyReloading) return;
            if (!File.Exists(AssemblyPath)) return;
            needsReload = true;
        }
    }

    internal class AssemblyLoadManager
    {

        public IReadOnlyList<ISystem> Systems => systems;
        public IReadOnlyList<LoadedAssembly> LoadedAssemblies => loadedAssemblies;

        public event Action<Assembly>? OnAssemblyLoad;
        public event Action? OnAssemblyUnload;

        private List<LoadedAssembly> loadedAssemblies = new();

        private List<ISystem> systems = new();

        public void RegisterAssembly(string path)
        {
            LoadedAssembly la = new(path, true);
            if (File.Exists(path))
            {
                la.Load();
                OnAssemblyLoad?.Invoke(la.LoadedAs!);
            }
            loadedAssemblies.Add(la);

            RediscoverSystems();
        }

        public void UnregisterAssembly(string path)
        {
            OnAssemblyUnload?.Invoke();
            LoadedAssembly la = 
                loadedAssemblies.Where((LoadedAssembly a) => a.AssemblyPath == path).First();
            
            systems.RemoveAll((ISystem system) => system.GetType().Assembly == la.LoadedAs);

            la.Unload();
            loadedAssemblies.Remove(la);

            RediscoverSystems();
        }

        public void ReloadAll()
        {
            OnAssemblyUnload?.Invoke();
            foreach (LoadedAssembly la in loadedAssemblies)
            {
                la.SwapReload();
                OnAssemblyLoad?.Invoke(la.LoadedAs!);
            }
            RediscoverSystems();
            Editor.Editor.Notify($"Reloaded {loadedAssemblies.Count} assembl{(loadedAssemblies.Count == 1 ? "y" : "ies")}");
        }

        public void ReloadIfNecessary()
        {
            bool reloadHappening = false;
            foreach (LoadedAssembly la in loadedAssemblies)
            {
                if (la.NeedsReload)
                    reloadHappening = true;
            }

            if (reloadHappening)
            {
                OnAssemblyUnload?.Invoke();
            }

            foreach (LoadedAssembly la in loadedAssemblies)
            {
                if (la.NeedsReload)
                {
                    la.ReloadIfNecessary();
                    OnAssemblyLoad?.Invoke(la.LoadedAs!);
                }
            }

            if (reloadHappening)
            {
                RediscoverSystems();
                Editor.Editor.Notify($"Reloaded {loadedAssemblies.Count} assembl{(loadedAssemblies.Count == 1 ? "y" : "ies")}");
            }
        }

        public Type? GetTypeFromAssemblies(string idStr)
        {
            foreach (LoadedAssembly la in loadedAssemblies)
            {
                if (la.LoadedAs == null) continue;

                Type? t = la.LoadedAs.GetType(idStr);

                if (t != null) return t;
            }
            
            return null;
        }

        private void RediscoverSystems()
        {
            // If the system type can't be found then
            // hotloading sets it to null.
            systems.RemoveAll((ISystem sys) => sys == null);
            foreach (LoadedAssembly la in loadedAssemblies)
            {
                Assembly? asm = la.LoadedAs;

                if (asm == null) continue;

                foreach (Type systemType in asm.GetTypes())
                {
                    if (!typeof(ISystem).IsAssignableFrom(systemType))
                    {
                        continue;
                    }

                    // Don't re-create systems we already have
                    // Just let hotloading take care of maintaining them across
                    // loads
                    bool alreadyExists = false;
                    foreach (ISystem existingSystem in systems)
                    {
                        if (existingSystem.GetType() == systemType)
                        {
                            alreadyExists = true;
                            break;
                        }
                    }

                    if (alreadyExists) continue;
                    
                    systems.Add((ISystem)Activator.CreateInstance(systemType)!);
                }
            }

            systems.OrderBy(s => s.GetType().GetCustomAttribute<SystemUpdateOrderAttribute>()?.Priority ?? 0);
        }
    }
}
