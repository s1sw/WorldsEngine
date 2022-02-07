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
    public class ProjectilePrism : Component
    {
        public int SplitCount = 3;
        public bool SingleUse = false;
        public AmmoType ProjectileType;

        public void RefractProjectile(Entity incomingEntity)
        {
            Transform thisTransform = Registry.GetTransform(Entity);
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(incomingEntity);
            Transform incomingTransform = dpa.Pose;

            for (int i = 0; i < SplitCount; i++)
            {
                Vector3 projectileDirection = new(Random.Shared.NextSingle() * 2f - 1f, Random.Shared.NextSingle() * 2f - 1f, 1f);
                projectileDirection.Normalize();
                projectileDirection = incomingTransform.TransformDirection(projectileDirection);
                Vector3 newVel = projectileDirection * Projectiles.GetProjectileSpeed(ProjectileType);

                Entity splitProjectile = Registry.CreatePrefab(Projectiles.GetProjectilePrefab(ProjectileType));

                Transform projectileTransform = new();
                projectileTransform.Rotation = Quaternion.SafeLookAt(projectileDirection);
                projectileTransform.Position = thisTransform.Position + projectileDirection;

                var newDpa = Registry.GetComponent<DynamicPhysicsActor>(splitProjectile);
                newDpa.Pose = projectileTransform;
                newDpa.Velocity = newVel;
            }

            if (SingleUse)
                Registry.DestroyNext(Entity);
        }
    }
}
