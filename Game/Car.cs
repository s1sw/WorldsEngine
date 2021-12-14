using System;
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
            // make. wheel.
        }

        public Entity SpawnWheel(Transform carRelativePose)
        {
            AssetID wheelPrefab = AssetDB.PathToId("Prefabs/wheel.wprefab");
            Entity wheel = Registry.CreatePrefab(wheelPrefab);
            return wheel;
        }

        public void Think(Entity e)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(e);

            // Simple approximation of a car - just add force+torque
            if (Accelerate)
                dpa.AddForce(dpa.Pose.Forward * 1000.0f);

            if (MathF.Abs(dpa.AngularVelocity.y) < 2 * MathF.PI * 4 || MathF.Sign(Steer) != MathF.Sign(dpa.AngularVelocity.y))
                dpa.AddTorque(dpa.Pose.TransformDirection(Vector3.Up) * Steer * 400.0f);

            // Here's an approximation for drag.
            // We're assuming that...
            const float surfaceArea = 1.5f * 1.695f;
            const float dragCoefficient = 0.298f;
            const float airDensity = 1.225f;

            float dragMagnitude = 0.5f * airDensity * dpa.Velocity.LengthSquared * dragCoefficient * surfaceArea;
            Vector3 dragForce = -dpa.Velocity.Normalized * dragMagnitude;

            dpa.AddForce(dragForce);
        }
    }
}
