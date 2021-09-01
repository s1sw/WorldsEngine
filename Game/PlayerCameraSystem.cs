using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;

namespace Game
{
    public class PlayerCameraSystem : ISystem
    {
        private float lookX = 0.0f;
        private float lookY = 0.0f;

        private Entity locosphereEntity;

        public void OnSceneStart()
        {
            locosphereEntity = Registry.Find("Player Locosphere");
            lookX = 0.0f;
            lookY = 0.0f;
        }

        public void OnUpdate()
        {
            if (locosphereEntity.IsNull || FreecamSystem.Enabled) return;

            Transform locosphereTransform = Registry.GetTransform(locosphereEntity);

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
                    Camera.Main.Position = bodyTransform.Position + new Vector3(0f, 0.5f * (bodyTransform.Scale.y / 0.75f), 0f);
                }
                else
                {
                    Camera.Main.Position = locosphereTransform.Position + new Vector3(0f, 1.6f, 0f);
                }
            }
            else
            {
                Vector3 hmdOffset = VR.HMDTransform.Position;
                hmdOffset.y = -0.15f;

                Camera.Main.Position = locosphereTransform.Position - (Camera.Main.Rotation * hmdOffset);
            }

            if (Keyboard.KeyPressed(KeyCode.L))
            {
                DynamicPhysicsActor dpa = Registry.GetComponent<DynamicPhysicsActor>(locosphereEntity);
                dpa.AddForce(new Vector3(1.0f, 0.0f, 0.0f), ForceMode.VelocityChange);
            }
        }
    }
}
