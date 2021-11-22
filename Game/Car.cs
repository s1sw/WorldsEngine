using WorldsEngine;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    class Car : IStartListener, IThinkingComponent
    {
        public bool Accelerate = false;
        public float Steer = 0.0f;

        public void Start(Entity e)
        {
        }

        public void Think(Entity e)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(e);

            // I honestly can't be bothered to fully simulate a car,
            // so let's go with a pretty simple approximation for now.
            // Acceleration just applies a force of 1000N.
            // Steering applies a magical torque from nowhere!
            if (Accelerate)
            {
                dpa.AddForce(dpa.Pose.Forward * 1000.0f);
            }

            ImGuiNET.ImGui.Text($"Steer: {Steer}");

            dpa.AddTorque(dpa.Pose.TransformDirection(Vector3.Up) * Steer * 400.0f);

            // Here's a really awful approximation for drag.
            // We're assuming a surface area of 4m,
            const float surfaceArea = 4.0f;
            const float dragCoefficient = 0.20f;
            const float airDensity = 1.225f;

            float dragMagnitude = 0.5f * airDensity * dpa.Velocity.LengthSquared * dragCoefficient * surfaceArea;
            Vector3 dragForce = -dpa.Velocity.Normalized * dragMagnitude;

            dpa.AddForce(dragForce);
        }
    }
}
