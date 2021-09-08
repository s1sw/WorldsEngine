using Game.Interaction;
using WorldsEngine;
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
            grabbable.TriggerPressed += Grabbable_TriggerPressed; ;
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
            var transform = Registry.GetTransform(entity);
            AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
            Entity projectile = Registry.CreatePrefab(projectileId);

            var projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);
            projectileDpa.AddForce(transform.TransformDirection(Vector3.Forward) * 100f, ForceMode.VelocityChange);

            Transform projectileTransform = transform;
            projectileTransform.Position += transform.Forward * 2.0f;
            Registry.SetTransform(entity, projectileTransform);
            projectileDpa.Pose = projectileTransform;
        }

        public void Think(Entity entity)
        {
            if (_shotTimer < ShotSpacing * 2f)
                _shotTimer += Time.DeltaTime;
        }
    }
}
