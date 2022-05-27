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
        public readonly string Path;
        public Assembly? LoadedAs { get; private set; }
        public AssemblyLoadContext? LoadContext { get; private set; }

        public bool Loaded { get; private set; }

        public LoadedAssembly(string path)
        {
            Path = path;
        }

        public void Load()
        {
            if (Loaded)
                throw new InvalidOperationException("Cannot load an assembly that is already loaded.");

            LoadContext = new AssemblyLoadContext(Path, true);
            LoadedAs = LoadContext.LoadFromAssemblyPath(Path);

            Log.Msg($"Loaded assembly {Path}");
        }

        public void Unload()
        {
            if (!Loaded)
                throw new InvalidOperationException("Cannot unload an assembly that is not loaded.");

            WeakReference weakRef = ClearLoadContext();
            EnsureAssemblyIsUnloaded(weakRef);

            Log.Msg($"Unloaded assembly {Path}");
        }

        public void SwapReload()
        {
            if (!Loaded)
                throw new InvalidOperationException("Cannot reload an assembly that is not loaded.");

            WeakReference weakRef = LoadSwap();
            EnsureAssemblyIsUnloaded(weakRef);
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
                    $"Failed to unload assembly {Path} after 20 iterations! " +
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
            AssemblyLoadContext newAlc = new(Path, true);
            Assembly newAssembly = newAlc.LoadFromAssemblyPath(Path);

            HotloadSwapper swapper = new(LoadedAs!, newAssembly);
            swapper.SwapReferences();

            WeakReference weakRef = ClearLoadContext();

            LoadedAs = newAssembly;
            LoadContext = newAlc;

            return weakRef;
        }
    }

    internal class AssemblyLoadManager
    {

        public IReadOnlyList<ISystem> Systems => systems;
        public IReadOnlyList<LoadedAssembly> LoadedAssemblies => loadedAssemblies;

        public event Action<Assembly>? OnAssemblyLoad;
        public event Action? OnAssemblyReload;
        public event Action? OnAssemblyUnload;

        private List<LoadedAssembly> loadedAssemblies = new();
        private List<ISystem> systems = new();

        public void RegisterAssembly(string path)
        {
            LoadedAssembly la = new(path);
            la.Load();
            loadedAssemblies.Add(la);

            OnAssemblyLoad?.Invoke(la.LoadedAs!);
            RediscoverSystems();
        }

        public void UnregisterAssembly(string path)
        {
            OnAssemblyUnload?.Invoke();
            LoadedAssembly la = 
                loadedAssemblies.Where((LoadedAssembly a) => a.Path == path).First();
            
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
        }
    }
}
