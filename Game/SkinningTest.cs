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

            for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
                var restPose = MeshManager.GetBoneRestPose(swo.Mesh, i);
                swo.SetBoneTransform(i, restPose);

                Entity entity = Registry.Create();
                WorldObject wo = Registry.AddComponent<WorldObject>(entity);
                wo.Mesh = AssetDB.PathToId("Models/sphere.wmdl");
                wo.SetMaterial(0, DevMaterials.Blue);

                restPose.Scale *= 0.1f;

                Registry.SetTransform(entity, restPose);
            }
            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone");

            _startRotation = MeshManager.GetBoneRestPose(swo.Mesh, boneIdx).Rotation;
            _startPosition = MeshManager.GetBoneRestPose(swo.Mesh, boneIdx).Position;
        }

        public void Think(Entity e)
        {
            return;
            var swo = Registry.GetComponent<SkinnedWorldObject>(e);

            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone");

            if (boneIdx != ~0u)
            {
                float rotationT = (float)Math.Sin(Time.CurrentTime) * 0.5f + 0.5f;
                Quaternion rotation = Quaternion.AngleAxis(rotationT * MathF.PI - (MathF.PI / 2.0f), Vector3.Left);
                Transform restT = new Transform(_startPosition, _startRotation);//new Transform(Vector3.Zero, _startRotation).TransformBy(new Transform(Vector3.Zero, rotation));
                swo.SetBoneTransform(boneIdx, restT);
            }
        }
    }
}
