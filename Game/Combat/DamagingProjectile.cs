using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Math;

namespace Game.Combat
{
    [Component]
    [EditorFriendlyName("C# Damaging Projectile")]
    public class DamagingProjectile : IStartListener, ICollisionHandler
    {
        public double Damage = 5.0;
        public double CreationTime = 0.0;
        public int BounceCount = 0;

        public void Start(Entity e)
        {
            CreationTime = Time.CurrentTime;
        }

        public void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo)
        {
            if (BounceCount == 0)
            {
                Registry.DestroyNext(entity);
            }
            else
            {
                BounceCount--;
                var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);

                dpa.Velocity = Vector3.Reflect(dpa.Velocity, contactInfo.Normal);

                var pose = dpa.Pose;
                pose.Rotation = Quaternion.SafeLookAt(dpa.Velocity.Normalized);
                dpa.Pose = pose;
            }

            if (!Registry.HasComponent<HealthComponent>(contactInfo.OtherEntity)) return;

            var health = Registry.GetComponent<HealthComponent>(contactInfo.OtherEntity);

            health.Damage(Damage);
        }
    }

    public class ProjectileCleanupSystem : ISystem
    {
        public void OnUpdate()
        {
            foreach (Entity projectileEntity in Registry.View<DamagingProjectile>())
            {
                var projectile = Registry.GetComponent<DamagingProjectile>(projectileEntity);

                if (Time.CurrentTime - projectile.CreationTime > 5.0)
                {
                    Registry.DestroyNext(projectileEntity);
                }
            }
        }
    }
}
