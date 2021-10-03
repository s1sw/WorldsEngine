using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public class WorldLight : BuiltinComponent
    {
        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool worldlight_getEnabled(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void worldlight_setEnabled(IntPtr registryPtr, uint entityId, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float worldlight_getIntensity(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void worldlight_setIntensity(IntPtr registryPtr, uint entityId, float intensity);

        public bool Enabled
        {
            get => worldlight_getEnabled(regPtr, entityId);
            set => worldlight_setEnabled(regPtr, entityId, value);
        }

        public float Intensity
        {
            get => worldlight_getIntensity(regPtr, entityId);
            set => worldlight_setIntensity(regPtr, entityId, value);
        }

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("World Light")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        internal WorldLight(IntPtr regPtr, uint entityId) : base(regPtr, entityId) {}
    }
}
