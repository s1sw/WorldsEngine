using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;

namespace Game
{
    [SystemUpdateOrder(-1)]
    public class PlayerCameraSystem : ISystem
    {
        private float lookX = 0.0f;
        private float lookY = 0.0f;

        private Entity _listenerEntity;
        private static readonly Vector3 _toFloor = new(0.0f, -(1.8f * 0.5f) - PlayerRig.HoverDistance, 0.0f);

        public void OnSceneStart()
        {
            lookX = 0.0f;
            lookY = 0.0f;

            if (VR.Enabled)
            {
                _listenerEntity = Registry.Create();
                Registry.SetName(_listenerEntity, "VR Audio Listener Override");

                Registry.AddComponent<AudioListenerOverride>(_listenerEntity);
            }
        }

        public static Vector3 GetCamPosForSimulation()
        {
            Transform bodyTransform = Registry.GetTransform(PlayerRigSystem.PlayerBody);
            var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(PlayerRigSystem.PlayerBody);

            if (!VR.Enabled)
            {
                return bodyDpa.Pose.Position + _toFloor + new Vector3(0.0f, 1.8f, 0.0f);

            }
            else
            {
                Vector3 hmdOffset = VRTransforms.HMDTransform.Position;
                hmdOffset.y = 0.0f;

                return bodyDpa.Pose.Position + PlayerRigSystem.VirtualRotation * (-hmdOffset) + _toFloor;
            }
        }

        public void OnUpdate()
        {
            if (PlayerRigSystem.PlayerBody.IsNull || FreecamSystem.Enabled) return;

            if (!VR.Enabled)
            {
                lookX += Mouse.PositionDelta.x * 0.005f;
                lookY += Mouse.PositionDelta.y * 0.005f;

                Quaternion upDown = Quaternion.AngleAxis(lookY, new Vector3(1f, 0f, 0f));
                Quaternion leftRight = Quaternion.AngleAxis(-lookX, new Vector3(0f, 1f, 0f));

                Quaternion cameraRotation = leftRight * upDown;


                if (Registry.Valid(PlayerRigSystem.PlayerBody))
                {
                    Camera.Main.Rotation = cameraRotation;
                    Transform bodyTransform = Registry.GetTransform(PlayerRigSystem.PlayerBody);
                    Camera.Main.Position = bodyTransform.Position + _toFloor + new Vector3(0.0f, 1.8f, 0.0f);
                }
            }
            else
            {
                Vector3 hmdOffset = VRTransforms.HMDTransform.Position;
                hmdOffset.y = 0.0f;

                Transform bodyTransform = Registry.GetTransform(PlayerRigSystem.PlayerBody);
                Camera.Main.Position = bodyTransform.Position + PlayerRigSystem.VirtualRotation * (-hmdOffset) + _toFloor;
                Camera.Main.Rotation = PlayerRigSystem.VirtualRotation;
                Transform listenerTransform = new();

                listenerTransform.Position = Camera.Main.Position + PlayerRigSystem.VirtualRotation * (VRTransforms.HMDTransform.Position);
                listenerTransform.Rotation = PlayerRigSystem.VirtualRotation * VRTransforms.HMDTransform.Rotation;
                listenerTransform.Scale = Vector3.One;

                Registry.SetTransform(_listenerEntity, listenerTransform);
            }
        }
    }
}
