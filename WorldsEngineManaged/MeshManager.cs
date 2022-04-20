using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public class LoadedMesh
    {
        [DllImport(WorldsEngine.NativeModule)]
        [return : MarshalAs(UnmanagedType.I1)]
        private static extern bool meshmanager_isMeshSkinned(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint meshmanager_getBoneId(uint meshId, string name);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void meshmanager_getBoneRestTransform(uint meshId, uint boneId, ref Transform t);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void meshmanager_getBoneRelativeTransform(uint meshId, uint boneId, ref Transform t);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint meshmanager_getBoneCount(uint meshId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float meshmanager_getSphereBoundRadius(uint meshId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint meshmanager_getBoneParent(uint meshId, uint boneId);

        public int BoneCount => (int)meshmanager_getBoneCount(_id.ID);
        public float SphereBoundRadius => meshmanager_getSphereBoundRadius(_id.ID);

        public int BoneParentIndex(int boneIndex)
        {
            uint v = meshmanager_getBoneParent(_id.ID, (uint)boneIndex);

            if (v == ~0u)
                return -1;

            return (int)v;
        }

        private AssetID _id;

        internal LoadedMesh(AssetID id)
        {
            _id = id;
        }
    }

    public static class MeshManager
    {
        [DllImport(WorldsEngine.NativeModule)]
        [return : MarshalAs(UnmanagedType.I1)]
        private static extern bool meshmanager_isMeshSkinned(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint meshmanager_getBoneId(uint meshId, string name);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void meshmanager_getBoneRestTransform(uint meshId, uint boneId, ref Transform t);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void meshmanager_getBoneRelativeTransform(uint meshId, uint boneId, ref Transform t);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint meshmanager_getBoneCount(uint meshId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float meshmanager_getSphereBoundRadius(uint meshId);

        public static bool IsMeshSkinned(AssetID id) => meshmanager_isMeshSkinned(id.ID);
        public static uint GetBoneIndex(AssetID id, string name) => meshmanager_getBoneId(id.ID, name);

        public static Transform GetBoneRestPose(AssetID id, uint boneId)
        {
            Transform t = new();
            meshmanager_getBoneRestTransform(id.ID, boneId, ref t);
            return t;
        }

        public static Transform GetBoneRestTransform(AssetID id, uint boneId)
        {
            Transform t = new();
            meshmanager_getBoneRelativeTransform(id.ID, boneId, ref t);
            return t;
        }

        public static int GetBoneCount(AssetID mesh) => (int)meshmanager_getBoneCount(mesh.ID);

        public static float GetMeshSphereBoundRadius(AssetID mesh) => meshmanager_getSphereBoundRadius(mesh.ID);

        public static LoadedMesh GetMesh(AssetID id) => new LoadedMesh(id);
    }
}
