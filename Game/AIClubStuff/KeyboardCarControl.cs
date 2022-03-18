using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    class KeyboardCarControl : Component, IThinkingComponent
    {
        public bool DHeld = false;
        public bool AHeld = false;
        public bool WHeld = false;

        public void Think()
        {
            var car = Registry.GetComponent<Car>(Entity);

            car.Acceleration = WHeld ? 1.0f : 0.0f;

            car.TargetAngularVelocity = 0.0f;
            car.TargetAngularVelocity += AHeld ? 1.0f : 0.0f;
            car.TargetAngularVelocity -= DHeld ? 1.0f : 0.0f;
        }
    }

    class KeyboardCarControlSystem : ISystem
    {
        public void OnUpdate()
        {
            foreach (Entity e in Registry.View<KeyboardCarControl>())
            {
                var kcc = Registry.GetComponent<KeyboardCarControl>(e);
                kcc.DHeld = Keyboard.KeyHeld(KeyCode.D);
                kcc.AHeld = Keyboard.KeyHeld(KeyCode.A);
                kcc.WHeld = Keyboard.KeyHeld(KeyCode.W);

                var pose = Registry.GetTransform(e);

                Camera.Main.Position = pose.Position - (pose.Forward * 7f) + Vector3.Up;
                Camera.Main.Rotation = Quaternion.SafeLookAt((pose.Position + pose.Forward * 1.5f - Camera.Main.Position).Normalized);
            }
        }
    }
}
