using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Math;

namespace Game.Player;

[Component]
class HMDFollower : Component, IThinkingComponent
{
    public bool FollowRightHand = false;

    public void Think()
    {
        Transform t = VRTransforms.HMDTransform;
        t.Scale = new Vector3(1.0f);
        Registry.SetTransform(Entity, t);
    }
}
