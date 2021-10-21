using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Util;

namespace Game
{
    [Component]
    public class SkinningTest : IStartListener, IThinkingComponent
    {
        private Quaternion _startRotation;
        private Vector3 _startPosition;

        public void Start(Entity e)
        {
            var swo = Registry.GetComponent<SkinnedWorldObject>(e);
            var transform = Registry.GetTransform(e);

            for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
                var restPose = MeshManager.GetBoneRestPose(swo.Mesh, i);
                swo.SetBoneTransform(i, restPose);
            }
            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "finger_index_2_l");

            if (boneIdx == ~0u) return;

            _startRotation = MeshManager.GetBoneRestPose(swo.Mesh, boneIdx).Rotation;
            _startPosition = MeshManager.GetBoneRestPose(swo.Mesh, boneIdx).Position;
        }

        public void Think(Entity e)
        {
            var swo = Registry.GetComponent<SkinnedWorldObject>(e);

            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "finger_index_2_l");

            if (boneIdx != ~0u)
            {
                float rotationT = (float)Math.Sin(Time.CurrentTime) * 0.5f + 0.5f;
                Quaternion rotation = Quaternion.AngleAxis(rotationT * MathF.PI - (MathF.PI / 2.0f), Vector3.Left);
                Transform t = new Transform(_startPosition, rotation * _startRotation);
                swo.SetBoneTransform(boneIdx, t);
            }
        }
    }
}
