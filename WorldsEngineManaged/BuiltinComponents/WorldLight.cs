using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public enum LightType {
        Point,
        Spot,
        Directional,
        Sphere,
        Tube
    };
    
    public class WorldLight : BuiltinComponent
    {
        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool worldlight_getEnabled(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_setEnabled(IntPtr registryPtr, uint entityId, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Engine.NativeModule)]
        private static extern float worldlight_getIntensity(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_setIntensity(IntPtr registryPtr, uint entityId, float intensity);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_setColor(IntPtr registryPtr, uint entityId, Vector3 color);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_getColor(IntPtr registryPtr, uint entityId, out Vector3 color);

        [DllImport(Engine.NativeModule)]
        private static extern float worldlight_getRadius(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_setRadius(IntPtr registryPtr, uint entityId, float radius);

        [DllImport(Engine.NativeModule)]
        private static extern LightType worldlight_getType(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldlight_setType(IntPtr registryPtr, uint entityId, LightType type);

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

        public Vector3 Color
        {
            get
            {
                worldlight_getColor(regPtr, entityId, out Vector3 color);
                return color;
            }

            set => worldlight_setColor(regPtr, entityId, value);
        }

        public float Radius
        {
            get => worldlight_getRadius(regPtr, entityId);
            set => worldlight_setRadius(regPtr, entityId, value);
        }

        public LightType Type
        {
            get => worldlight_getType(regPtr, entityId);
            set => worldlight_setType(regPtr, entityId, value);
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
