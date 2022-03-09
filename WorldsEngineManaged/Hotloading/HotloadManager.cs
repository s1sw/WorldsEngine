using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Reflection.Metadata;

namespace WorldsEngine
{
    class HotloadManager
    {
        public IReadOnlyList<ISystem> Systems => _assemblyManager.Systems;

        private readonly FileSystemWatcher _dllWatcher;
        private readonly GameAssemblyManager _assemblyManager;
        private DateTime _lastReloadTime;
        private bool _reloadAssemblyNextFrame = false;

        public bool Active
        {
            set
            {
                _dllWatcher.EnableRaisingEvents = value;
                _active = value;
            }

            get => _active;
        }

        private bool _active = false;

        public HotloadManager()
        {
            _assemblyManager = new GameAssemblyManager();
            _assemblyManager.LoadGameAssembly();

            string watchPath = Path.GetFullPath("GameAssemblies");

#if Linux
            watchPath = Mono.Unix.UnixPath.GetCompleteRealPath(watchPath);
#endif

            _dllWatcher = new FileSystemWatcher(watchPath)
            {
                Filter = "",
                NotifyFilter = NotifyFilters.Attributes
                             | NotifyFilters.CreationTime
                             | NotifyFilters.LastAccess
                             | NotifyFilters.LastWrite
                             | NotifyFilters.Size,

                IncludeSubdirectories = true
            };

            _dllWatcher.Changed += OnDLLChanged;
            _dllWatcher.Renamed += OnDLLChanged;

            // Clear out all the temp game assembly directories
            foreach (string d in Directory.GetDirectories("."))
            {
                if (d.StartsWith("GameAssembliesTemp"))
                {
                    Directory.Delete(d, true);
                }
            }
        }

        private void OnDLLChanged(object sender, FileSystemEventArgs e)
        {
            if ((DateTime.Now - _lastReloadTime).TotalMilliseconds < 500)
            {
                return;
            }

            _lastReloadTime = DateTime.Now;
            _reloadAssemblyNextFrame = true;
            Logger.Log("DLL changed, reloading...");
        }

        public void ReloadIfNecessary()
        {
            if (!_active) return;

            if (_reloadAssemblyNextFrame || _assemblyManager.Assembly == null)
            {
                Registry.SerializeStorages();
                _assemblyManager.ReloadGameAssembly();
                _reloadAssemblyNextFrame = false;
            }
        }

        public void ForceReload()
        {
            _reloadAssemblyNextFrame = true;
        }
    }
}
