using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;

namespace Game
{
    public class PlayerCameraSystem : ISystem
    {
        private Entity locosphereEntity;

        public void OnSceneStart()
        {
            locosphereEntity = Registry.Find("Player Locosphere");
        }

        public void OnUpdate()
        {
            if (locosphereEntity.IsNull || FreecamSystem.Enabled) return;

            Transform locosphereTransform = Registry.GetTransform(locosphereEntity);


            if (!VR.Enabled)
            {
                Camera.Main.Position = locosphereTransform.Position + new Vector3(0.0f, 1.6f - 0.15f, 0.0f);
            }
            else
            {
                Vector3 hmdOffset = VR.HMDTransform.Position;
                hmdOffset.y = -0.15f;

                Camera.Main.Position = locosphereTransform.Position + (Camera.Main.Rotation * hmdOffset);
            }

            if (Keyboard.KeyPressed(KeyCode.L))
            {
                DynamicPhysicsActor dpa = Registry.GetComponent<DynamicPhysicsActor>(locosphereEntity);
                dpa.AddForce(new Vector3(1.0f, 0.0f, 0.0f), ForceMode.VelocityChange);
            }
        }
    }
}
