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
    class HMDFollower : IThinkingComponent
    {
        public bool FollowRightHand = false;

        public void Think(Entity entity)
        {
            Transform t = VR.HMDTransform;
            t.Scale = new Vector3(1.0f);
            Registry.SetTransform(entity, t);
        }
    }
}
