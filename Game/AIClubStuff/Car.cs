using WorldsEngine;
using WorldsEngine.Math;

namespace Game;

[Component]
class Car : Component, IThinkingComponent
{
    public float Acceleration = 0f;
    public float TargetAngularVelocity = 0f;

    public void Think()
    {
        var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);

        // Simple approximation of a car - just add force+torque
        dpa.AddForce(dpa.Pose.Forward * 100.0f * Acceleration, ForceMode.Acceleration);

        float angVelDif = (TargetAngularVelocity - dpa.AngularVelocity.y);
        dpa.AddTorque(Vector3.Up * angVelDif * 50.0f);

        // Here's an approximation for drag.
        // We're assuming that...
        const float surfaceArea = 1.5f * 1.695f;
        const float dragCoefficient = 0.298f;
        const float airDensity = 1.225f;

        float dragMagnitude = 0.5f * airDensity * dpa.Velocity.LengthSquared * dragCoefficient * surfaceArea;
        if (!dpa.Velocity.IsZero)
        {
            Vector3 dragForce = -dpa.Velocity.Normalized * dragMagnitude;
            dpa.AddForce(dragForce);
        }
    }
}
