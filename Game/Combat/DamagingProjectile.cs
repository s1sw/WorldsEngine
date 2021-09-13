using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game.Combat
{
    [Component]
    [EditorFriendlyName("C# Damaging Projectile")]
    public class DamagingProjectile : IStartListener, ICollisionHandler
    {
        public double Damage = 5.0;
        public double CreationTime = 0.0;

        public void Start(Entity e)
        {
            CreationTime = Time.CurrentTime;
        }

        public void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo)
        {
            Registry.DestroyNext(entity);
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
