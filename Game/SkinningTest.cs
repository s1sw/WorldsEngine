using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    public class SkinningTest : IStartListener, IThinkingComponent
    {
        private Quaternion _startRotation;
        public void Start(Entity e)
        {
            var swo = Registry.GetComponent<SkinnedWorldObject>(e);

            for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
                swo.SetBoneTransform(i, MeshManager.GetBoneRestPose(swo.Mesh, i));
            }
            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone.001");

            _startRotation = MeshManager.GetBoneRestPose(swo.Mesh, boneIdx).Rotation;
        }

        public void Think(Entity e)
        {
            //return;
            var swo = Registry.GetComponent<SkinnedWorldObject>(e);

            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone.001");

            if (boneIdx != ~0u)
            {
                float rotationT = (float)Math.Sin(Time.CurrentTime) * 0.5f + 0.5f;
                Quaternion rotation = Quaternion.AngleAxis(rotationT * MathF.PI - (MathF.PI / 2.0f), Vector3.Left);
                swo.SetBoneTransform(boneIdx, new Transform(Vector3.Zero, _startRotation * rotation));
            }
        }
    }
}
