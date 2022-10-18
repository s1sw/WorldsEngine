using System;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;
using WorldsEngine.ECS;

namespace WorldsEngine
{
    public class SkinnedWorldObject : BuiltinComponent
    {
        [DllImport(Engine.NativeModule)]
        private static extern uint skinnedWorldObject_getMesh(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void skinnedWorldObject_setMesh(IntPtr registryPtr, uint entityId, uint meshId);

        [DllImport(Engine.NativeModule)]
        private static extern uint skinnedWorldObject_getMaterial(IntPtr registryPtr, uint entityId, uint materialIndex);

        [DllImport(Engine.NativeModule)]
        private static extern void skinnedWorldObject_setMaterial(IntPtr registryPtr, uint entityId, uint materialIndex, uint material);

        [DllImport(Engine.NativeModule)]
        private static extern char skinnedWorldObject_exists(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void skinnedWorldObject_getBoneTransform(IntPtr registryPtr, uint entityId, uint boneIdx, ref Transform t);

        [DllImport(Engine.NativeModule)]
        private static extern void skinnedWorldObject_setBoneTransform(IntPtr registryPtr, uint entityId, uint boneIdx, ref Transform t);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("Skinned World Object")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        const int MAX_MATERIALS = 32;

        public AssetID Mesh
        {
            get => new AssetID(skinnedWorldObject_getMesh(regPtr, entityId));
            set => skinnedWorldObject_setMesh(regPtr, entityId, value.ID);
        }

        internal SkinnedWorldObject(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }

        public void SetMaterial(uint idx, AssetID id)
        {
            if (idx >= MAX_MATERIALS)
                throw new ArgumentOutOfRangeException($"Objects can have a maximum of 32 materials. Index {idx} is out of range (0 <= idx < 32)");

            skinnedWorldObject_setMaterial(regPtr, entityId, idx, id.ID);
        }

        public AssetID GetMaterial(uint idx)
        {
            if (idx >= MAX_MATERIALS)
                throw new ArgumentOutOfRangeException($"Objects can have a maximum of 32 materials. Index {idx} is out of range (0 <= idx < 32)");

            return new AssetID(skinnedWorldObject_getMaterial(regPtr, entityId, idx));
        }

        public Transform GetBoneTransform(uint boneIdx)
        {
            Transform t = new();
            skinnedWorldObject_getBoneTransform(regPtr, entityId, boneIdx, ref t);
            return t;
        }

        public Transform GetBoneComponentSpaceTransform(uint boneIdx)
        {
            var mesh = MeshManager.GetMesh(Mesh);
            var b = mesh.GetBone((int)boneIdx);

            Transform t = GetBoneTransform(b.ID);
            int parentIdx = b.Parent;
            while (parentIdx != -1)
            {
                Bone b2 = mesh.GetBone(parentIdx);
                t = t.TransformBy(GetBoneTransform(b2.ID));
                parentIdx = b2.Parent;
            }

            return t;
        }

        public void SetBoneTransform(uint boneIdx, Transform t)
        {
            skinnedWorldObject_setBoneTransform(regPtr, entityId, boneIdx, ref t);
        }

        public void SetBoneWorldSpaceTransform(uint boneIdx, Transform t)
        {
            SetBoneTransform(boneIdx, t.TransformByInverse(Registry.GetTransform(new Entity(entityId))).TransformByInverse(GetBoneComponentSpaceTransform((uint)MeshManager.GetMesh(Mesh).GetBone((int)boneIdx).Parent)));
        }

        public Transform GetBoneWorldSpaceTransform(uint boneIdx)
        {
            return GetBoneComponentSpaceTransform(boneIdx).TransformBy(Registry.GetTransform(new Entity(entityId)));
        }
    }
}
