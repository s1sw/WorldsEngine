using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public class AudioSource : BuiltinComponent
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint audiosource_getClipId(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audiosource_setClipId(IntPtr registryPtr, uint entityId, uint clipId);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool audiosource_getIsPlaying(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audiosource_setIsPlaying(IntPtr registryPtr, uint entityId, [MarshalAs(UnmanagedType.I1)] bool val);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float audiosource_getVolume(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audiosource_setVolume(IntPtr registryPtr, uint entityId, float volume);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("Audio Source")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        public AssetID ClipId
        {
            get => new AssetID(audiosource_getClipId(regPtr, entityId));
            set => audiosource_setClipId(regPtr, entityId, value.ID);
        }

        public bool IsPlaying
        {
            get => audiosource_getIsPlaying(regPtr, entityId);
            set => audiosource_setIsPlaying(regPtr, entityId, value);
        }

        public float Volume
        {
            get => audiosource_getVolume(regPtr, entityId);
            set => audiosource_setVolume(regPtr, entityId, value);
        }

        internal AudioSource(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }
    }
}
