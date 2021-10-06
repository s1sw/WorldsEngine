using Game.Interaction;
using System;
using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;
using Game.Combat;

namespace Game
{
    public enum ProjectileType
    {
        Laser,
        Humongous
    }

    [Component]
    [EditorFriendlyName("C# Gun")]
    class Gun : IThinkingComponent, IStartListener, ICollisionHandler
    {
        public bool Automatic = false;
        public float ShotSpacing = 0.1f;
        public float ProjectileSpawnDistance = 0.5f;
        public ProjectileType ProjectileType;
        public bool MagazineRequired = false;
        public Vector3 MagazineAttachedPosition;

        private float _shotTimer = 0f;
        private AssetID _projectilePrefab;
        private bool _hasMagazine = false;
        private Entity _currentMagazine = Entity.Null;

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
            if (MagazineRequired)
            {
                if (!_hasMagazine) return;

                var mag = Registry.GetComponent<Magazine>(_currentMagazine);
                if (mag.NumShots <= 0) return;

                mag.NumShots--;

                if (mag.NumShots <= 0)
                {
                    EjectMag();
                }
            }

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

            Audio.PlayOneShotEvent(evt, transform.Position);

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

                DoHaptics(grabbable.AttachedHandFlags);
            }

            var damagingProjectile = Registry.GetComponent<DamagingProjectile>(entity);
            damagingProjectile.Attacker = PlayerRigSystem.PlayerBody;
        }

        public void Think(Entity entity)
        {
            if (_shotTimer < ShotSpacing * 2f)
                _shotTimer += Time.DeltaTime;
        }

        private async void DoHaptics(AttachedHandFlags handFlags)
        {
            switch (ProjectileType)
            {
                case ProjectileType.Laser:
                    HapticManager.Trigger(handFlags, 0.0f, MathF.Min(ShotSpacing, 0.2f), 50f, 1.0f);
                    break;
                case ProjectileType.Humongous:
                    HapticManager.Trigger(handFlags, 0.0f, 0.1f, 500f, 1.0f);
                    await System.Threading.Tasks.Task.Delay(100);
                    HapticManager.Trigger(handFlags, 0.0f, 0.1f, 200f, 1.0f);
                    await System.Threading.Tasks.Task.Delay(100);
                    HapticManager.Trigger(handFlags, 0.0f, 0.1f, 50f, 1.0f);
                    break;
            }
        }

        public void OnCollision(Entity entity, ref PhysicsContactInfo contactInfo)
        {
            if (!Registry.HasComponent<Magazine>(contactInfo.OtherEntity) || contactInfo.OtherEntity == _currentMagazine) return;

            if (_hasMagazine)
            {
                EjectMag();
            }

            _hasMagazine = true;

            var d6 = Registry.AddComponent<D6Joint>(contactInfo.OtherEntity);
            d6.Target = entity;
            d6.TargetLocalPose = new Transform(MagazineAttachedPosition, Quaternion.Identity);

            d6.SetAllAxisMotion(D6Motion.Locked);
            _currentMagazine = contactInfo.OtherEntity;
        }

        public void EjectMag()
        {
            if (!_hasMagazine) return;

            _hasMagazine = false;
            Registry.RemoveComponent<D6Joint>(_currentMagazine);

            var dpa = Registry.GetComponent<DynamicPhysicsActor>(_currentMagazine);
            dpa.AddForce(dpa.Pose.TransformDirection(Vector3.Left), ForceMode.VelocityChange);

            _currentMagazine = Entity.Null;
        }
    }
}
