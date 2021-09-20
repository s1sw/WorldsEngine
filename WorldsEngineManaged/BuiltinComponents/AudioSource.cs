using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public enum PlaybackState : int
    {
        Playing,
        Sustaining,
        Stopped,
        Starting,
        Stopping
    }

    public enum StopMode : int
    {
        AllowFadeout,
        Immediate
    }

    public class AudioSource : BuiltinComponent
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audiosource_start(IntPtr regPtr, uint entity);

        [DllImport(WorldsEngine.NativeModule)] 
        private static extern void audiosource_stop(IntPtr regPtr, uint entity, StopMode stopMode);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern PlaybackState audiosource_getPlayState(IntPtr regPtr, uint entity);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audiosource_setParameter(IntPtr regPtr, uint entity, string parameterName, float value);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("FMOD Audio Source")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        public PlaybackState PlaybackState => audiosource_getPlayState(regPtr, entityId);

        internal AudioSource(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }

        public void Start()
        {
            audiosource_start(regPtr, entityId);
        }

        public void Stop(StopMode stopMode)
        {
            audiosource_stop(regPtr, entityId, stopMode);
        }

        public void SetParameter(string parameterName, float value)
        {
            audiosource_setParameter(regPtr, entityId, parameterName, value);
        }
    }
}
