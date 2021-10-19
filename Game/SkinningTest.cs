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
    public class SkinningTest : IThinkingComponent
    {
        public void Think(Entity e)
        {
            if (!Registry.HasComponent<SkinnedWorldObject>(e)) return;

            var swo = Registry.GetComponent<SkinnedWorldObject>(e);

            uint boneIdx = MeshManager.GetBoneIndex(swo.Mesh, "Bone.003");

            if (boneIdx != ~0u)
            {
                Quaternion rotation = Quaternion.AngleAxis((float)Math.Sin(Time.CurrentTime), Vector3.Left);
                swo.SetBoneTransform(boneIdx, new Transform(Vector3.Zero, rotation));
            }
        }
    }
}
