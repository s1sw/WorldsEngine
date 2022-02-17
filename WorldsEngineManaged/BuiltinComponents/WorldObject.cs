using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;

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
        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint worldObject_getMesh(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void worldObject_setMesh(IntPtr registryPtr, uint entityId, uint meshId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint worldObject_getMaterial(IntPtr registryPtr, uint entityId, uint materialIndex);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void worldObject_setMaterial(IntPtr registryPtr, uint entityId, uint materialIndex, uint material);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern char worldObject_exists(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern byte worldObject_getStaticFlags(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void worldObject_setStaticFlags(IntPtr registryPtr, uint entityId, byte flags);

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
