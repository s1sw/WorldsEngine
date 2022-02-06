using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game.Combat;

[Component]
public class ImpactDamageDealer : ICollisionHandler
{
    public double Damage = 5.0f;
    public float MinimumVelocity = 2.0f;

    public void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo)
    {
        if (!Registry.TryGetComponent<HealthComponent>(contactInfo.OtherEntity, out var healthComponent)) return;

        // Calculate difference in velocity, taking into account angular velocity
        var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);
        var localSpaceContactPoint = dpa.Pose.InverseTransformPoint(contactInfo.AverageContactPoint);
        var pointVelocity = (localSpaceContactPoint * dpa.AngularVelocity) + dpa.Velocity;

        if (pointVelocity.LengthSquared < MinimumVelocity * MinimumVelocity) return;
        if (contactInfo.OtherEntity == LocalPlayerSystem.PlayerBody) return;

        healthComponent.Damage(Damage, LocalPlayerSystem.PlayerBody);
    }
}
