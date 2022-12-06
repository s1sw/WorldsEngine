using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;
using WorldsEngine.Math;

namespace WorldsEngine
{
    [Flags]
    public enum StaticFlags : byte
    {
        None = 0,
        Audio = 1,
        Rendering = 2,
        Navigation = 4
    }

    public enum UVOverride
    {
        None,
        XY,
        XZ,
        ZY,
        PickBest
    }

    public class WorldObject : BuiltinComponent
    {
        [DllImport(Engine.NativeModule)]
        private static extern uint worldObject_getMesh(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_setMesh(IntPtr registryPtr, uint entityId, uint meshId);

        [DllImport(Engine.NativeModule)]
        private static extern uint worldObject_getMaterial(IntPtr registryPtr, uint entityId, uint materialIndex);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_setMaterial(IntPtr registryPtr, uint entityId, uint materialIndex, uint material);

        [DllImport(Engine.NativeModule)]
        private static extern char worldObject_exists(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern byte worldObject_getStaticFlags(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_setStaticFlags(IntPtr registryPtr, uint entityId, byte flags);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_getUvOffset(IntPtr registryPtr, uint entityId, ref Vector2 offset);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_setUvOffset(IntPtr registryPtr, uint entityId, ref Vector2 offset);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_getUvScale(IntPtr registryPtr, uint entityId, ref Vector2 scale);

        [DllImport(Engine.NativeModule)]
        private static extern void worldObject_setUvScale(IntPtr registryPtr, uint entityId, ref Vector2 scale);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("World Object")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        const int MAX_MATERIALS = 32;

        public AssetID Mesh
        {
            get => new AssetID(worldObject_getMesh(regPtr, entityId));
            set => worldObject_setMesh(regPtr, entityId, value.ID);
        }

        public StaticFlags StaticFlags
        {
            get => (StaticFlags)worldObject_getStaticFlags(regPtr, entityId);
            set => worldObject_setStaticFlags(regPtr, entityId, (byte)value);
        }

        public Vector2 UVScale
        {
            get
            {
                Vector2 scale = new();
                worldObject_getUvScale(regPtr, entityId, ref scale);
                return scale;
            }

            set => worldObject_setUvScale(regPtr, entityId, ref value);
        }

        public Vector2 UVOffset
        {
            get
            {
                Vector2 scale = new();
                worldObject_getUvOffset(regPtr, entityId, ref scale);
                return scale;
            }

            set => worldObject_setUvOffset(regPtr, entityId, ref value);
        }

        internal WorldObject(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }

        public void SetMaterial(uint idx, AssetID id)
        {
            if (idx >= MAX_MATERIALS)
                throw new ArgumentOutOfRangeException($"Objects can have a maximum of 32 materials. Index {idx} is out of range (0 <= idx < 32)");

            worldObject_setMaterial(regPtr, entityId, idx, id.ID);
        }

        public AssetID GetMaterial(uint idx)
        {
            if (idx >= MAX_MATERIALS)
                throw new ArgumentOutOfRangeException($"Objects can have a maximum of 32 materials. Index {idx} is out of range (0 <= idx < 32)");

            return new AssetID(worldObject_getMaterial(regPtr, entityId, idx));
        }
    }
}
