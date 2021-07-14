using System;
using System.Runtime.InteropServices;

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
        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern uint worldObject_getMesh(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern void worldObject_setMesh(IntPtr registryPtr, uint entityId, uint meshId);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern uint worldObject_getMaterial(IntPtr registryPtr, uint entityId, uint materialIndex);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern void worldObject_setMaterial(IntPtr registryPtr, uint entityId, uint materialIndex, uint material);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern char worldObject_exists(IntPtr registryPtr, uint entityId);

        internal static bool ExistsOn(IntPtr regPtr, Entity entity)
        {
            return worldObject_exists(regPtr, entity.ID) == 1;
        }

        const int MAX_MATERIALS = 32;

        public AssetID mesh
        {
            get
            {
                return new AssetID(worldObject_getMesh(regPtr, entityId));
            }

            set
            {
                worldObject_setMesh(regPtr, entityId, value.ID);
            }
        }

        private readonly IntPtr regPtr;
        private readonly uint entityId;

        internal WorldObject(IntPtr regPtr, uint entityId)
        {
            this.regPtr = regPtr;
            this.entityId = entityId;
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
