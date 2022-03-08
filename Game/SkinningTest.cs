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
    public class SkinningTest : Component, IStartListener, IThinkingComponent
    {
        private Quaternion _startRotation;
        private Vector3 _startPosition;

        public void Start()
        {
            var swo = Registry.GetComponent<SkinnedWorldObject>(Entity);
            var transform = Registry.GetTransform(Entity);

            for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
                var restPose = MeshManager.GetBoneRestTransform(swo.Mesh, i);

                if (!restPose.Rotation.Valid)
                {
                    Log.Error($"Rest pose rotation was invalid!! {restPose.Rotation}");
                    restPose.Rotation = Quaternion.Identity;
                }

                swo.SetBoneTransform(i, restPose);
            }
            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone.001");

            if (boneIdx == ~0u)
            {
                boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "finger_index_0_l");
            }

            if (boneIdx == ~0u) return;

            _startRotation = MeshManager.GetBoneRestTransform(swo.Mesh, boneIdx).Rotation;
            _startPosition = MeshManager.GetBoneRestTransform(swo.Mesh, boneIdx).Position;
            Logger.Log($"startPos: {_startPosition}, startRot: {_startRotation}");
        }

        public void Think()
        {
            var swo = Registry.GetComponent<SkinnedWorldObject>(Entity);

            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "finger_index_0_l");

            if (boneIdx != ~0u)
            {
                float rotationT = (float)Math.Sin(Time.CurrentTime) * 0.5f + 0.5f;
                Quaternion rotation = Quaternion.AngleAxis(rotationT * MathF.PI - (MathF.PI / 2.0f), Vector3.Forward);
                Transform extraT = new Transform(Vector3.Zero, rotation);
                Transform t = extraT.TransformBy(new Transform(_startPosition, _startRotation));
                swo.SetBoneTransform(boneIdx, t);
            }
        }
    }
}
