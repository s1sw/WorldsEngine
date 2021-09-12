using Game.Interaction;
using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    [EditorFriendlyName("C# Gun")]
    class Gun : IThinkingComponent, IStartListener
    {
        public bool Automatic = false;
        public float ShotSpacing = 0.1f;

        private float _shotTimer = 0f;

        public void Start(Entity entity)
        {
            var grabbable = Registry.GetComponent<Grabbable>(entity);
            grabbable.TriggerPressed += Grabbable_TriggerPressed;
            grabbable.TriggerHeld += Grabbable_TriggerHeld;
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
            projectileTransform.Position += transform.Forward * 0.5f;

            Audio.PlayOneShot(AssetDB.PathToId("Audio/SFX/gunshot.ogg"), projectileTransform.Position, 1.0f);

            AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
            Entity projectile = Registry.CreatePrefab(projectileId);

            var projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);
            projectileDpa.AddForce(transform.TransformDirection(Vector3.Forward) * 100f, ForceMode.VelocityChange);

            Registry.SetTransform(entity, projectileTransform);
            projectileDpa.Pose = projectileTransform;

            dpa.AddForce(-transform.TransformDirection(Vector3.Forward) * 100f * projectileDpa.Mass, ForceMode.Impulse);
        }

        public void Think(Entity entity)
        {
            if (_shotTimer < ShotSpacing * 2f)
                _shotTimer += Time.DeltaTime;
        }
    }
}
