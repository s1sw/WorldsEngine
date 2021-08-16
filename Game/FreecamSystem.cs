using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game
{
    public class FreecamSystem : ISystem
    {
        private float lookX = 0.0f;
        private float lookY = 0.0f;

        public static bool Enabled { get; private set; } = false;

        [ConsoleCommand("toggleFreecam", "Toggles the free camera.")]
        private static void ToggleFreeCam(string args)
        {
            Enabled = !Enabled;
        }

        public void OnUpdate()
        {
            if (!Enabled) return;

            Vector3 direction = new Vector3();

            if (Keyboard.KeyHeld(KeyCode.W))
                direction.z += 1.0f;

            if (Keyboard.KeyHeld(KeyCode.S))
                direction.z -= 1.0f;

            if (Keyboard.KeyHeld(KeyCode.A))
                direction.x += 1.0f;

            if (Keyboard.KeyHeld(KeyCode.D))
                direction.x -= 1.0f;

            float speed = 3.0f;

            if (Keyboard.KeyHeld(KeyCode.LeftShift))
            {
                speed *= 2.0f;
            }

            lookX += Mouse.PositionDelta.x * 0.005f;
            lookY += Mouse.PositionDelta.y * 0.005f;

            Quaternion upDown = Quaternion.AngleAxis(lookY, new Vector3(1.0f, 0.0f, 0.0f));
            Quaternion leftRight = Quaternion.AngleAxis(-lookX, new Vector3(0.0f, 1.0f, 0.0f));

            Quaternion cameraRotation = leftRight * upDown;

            Camera.Main.Rotation = cameraRotation;
            Camera.Main.Position += (cameraRotation * direction * Time.DeltaTime * speed);
        }
    }
}
