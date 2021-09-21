using Game.Interaction;
using System;
using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;

namespace Game
{
    public enum ProjectileType
    {
        Laser,
        Humongous
    }

    [Component]
    [EditorFriendlyName("C# Gun")]
    class Gun : IThinkingComponent, IStartListener
    {
        public bool Automatic = false;
        public float ShotSpacing = 0.1f;
        public float ProjectileSpawnDistance = 0.5f;
        public ProjectileType ProjectileType;

        private float _shotTimer = 0f;
        private AssetID _projectilePrefab;

        public void Start(Entity entity)
        {
            var grabbable = Registry.GetComponent<Grabbable>(entity);

            grabbable.TriggerPressed += Grabbable_TriggerPressed;
            grabbable.TriggerHeld += Grabbable_TriggerHeld;

            _projectilePrefab = ProjectileType switch
            {
                ProjectileType.Humongous => AssetDB.PathToId("Prefabs/big_ass_projectile.wprefab"),
                _ => AssetDB.PathToId("Prefabs/gun_projectile.wprefab"),
            };
        }

        private void Grabbable_TriggerPressed(Entity entity)
        {
            if (!Automatic && _shotTimer > ShotSpacing)
                Fire(entity);
        }

        private void Grabbable_TriggerHeld(Entity entity)
        {
            if (Automatic && _shotTimer > ShotSpacing)
                Fire(entity);
        }

        public void Fire(Entity entity)
        {
            _shotTimer = 0f;

            var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);
            var transform = dpa.Pose;

            Transform projectileTransform = transform;
            projectileTransform.Position += transform.Forward * ProjectileSpawnDistance;

            string evt = ProjectileType switch
            {
                ProjectileType.Humongous => "event:/Weapons/Big Gun",
                _ => "event:/Weapons/Gun Shot"
            };

            Audio.PlayOneShotEvent(evt, projectileTransform.Position);

            Entity projectile = Registry.CreatePrefab(_projectilePrefab);

            float speed = ProjectileType switch
            {
                ProjectileType.Humongous => 10f,
                _ => 100f
            };

            var projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);
            projectileDpa.AddForce(transform.TransformDirection(Vector3.Forward) * speed, ForceMode.VelocityChange);

            Registry.SetTransform(entity, projectileTransform);
            projectileDpa.Pose = projectileTransform;

            dpa.AddForce(-transform.TransformDirection(Vector3.Forward) * speed * projectileDpa.Mass, ForceMode.Impulse);

            if (Registry.HasComponent<Grabbable>(entity))
            {
                var grabbable = Registry.GetComponent<Grabbable>(entity);

                HapticManager.Trigger(grabbable.AttachedHandFlags, 0.0f, MathF.Min(ShotSpacing, 0.2f), 50f, 1.0f);
            }
        }

        public void Think(Entity entity)
        {
            if (_shotTimer < ShotSpacing * 2f)
                _shotTimer += Time.DeltaTime;
        }
    }
}
