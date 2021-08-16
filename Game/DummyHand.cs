using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    class DummyHand : IThinkingComponent
    {
        static Vector3 _nonVROffset = new Vector3(0.1f, -0.2f, 0.55f);

        public bool FollowRightHand = false;

        private Quaternion rotationOffset = Quaternion.Identity;

        public void Think(Entity entity)
        {
            if (!VR.Enabled)
            {
                Transform target = new Transform();
                target.Rotation = Camera.Main.Rotation;
                Vector3 offset = _nonVROffset;

                if (FollowRightHand)
                    offset.x *= -1.0f;

                target.Position = Camera.Main.TransformPoint(offset);
                target.Scale = new Vector3(1.0f);
                Registry.SetTransform(entity, target);
                return;
            }

            Transform t = FollowRightHand ? VR.RightHandTransform : VR.LeftHandTransform;

            if (Keyboard.KeyPressed(KeyCode.L) && !FollowRightHand)
            {
                rotationOffset = VR.LeftHandTransform.Rotation.Inverse;
                Logger.Log($"New rotation offset for left hand: {rotationOffset}");
            }

            if (Keyboard.KeyPressed(KeyCode.R) && FollowRightHand)
            {
                rotationOffset = VR.RightHandTransform.Rotation.Inverse;
                Logger.Log($"New rotation offset for right hand: {rotationOffset}");
            }

            t.Scale = new Vector3(1.0f);
            t.Rotation *= rotationOffset;
            Registry.SetTransform(entity, t);
        }
    }
}
