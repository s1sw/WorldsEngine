using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
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
        private static extern uint meshmanager_getBoneCount(uint meshId);

        public static bool IsMeshSkinned(AssetID id) => meshmanager_isMeshSkinned(id.ID);
        public static uint GetBoneIndex(AssetID id, string name) => meshmanager_getBoneId(id.ID, name);

        public static Transform GetBoneRestPose(AssetID id, uint boneId)
        {
            Transform t = new();
            meshmanager_getBoneRestTransform(id.ID, boneId, ref t);
            return t;
        }

        public static int GetBoneCount(AssetID mesh) => (int)meshmanager_getBoneCount(mesh.ID);
    }
}
