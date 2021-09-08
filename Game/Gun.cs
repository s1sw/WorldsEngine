using Game.Interaction;
using WorldsEngine;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    [EditorFriendlyName("C# Gun")]
    class Gun : IStartListener
    {
        public void Start(Entity entity)
        {
            var grabbable = Registry.GetComponent<Grabbable>(entity);
            grabbable.TriggerPressed += Fire;
            Logger.Log("Registered trigger press event");
        }

        public void Fire(Entity entity)
        {
            Logger.Log("Fire!!!");
            var transform = Registry.GetTransform(entity);
            AssetID projectileId = AssetDB.PathToId("Prefabs/gun_projectile.wprefab");
            Entity projectile = Registry.CreatePrefab(projectileId);

            var projectileDpa = Registry.GetComponent<DynamicPhysicsActor>(projectile);
            projectileDpa.AddForce(transform.TransformDirection(Vector3.Forward) * 100f);

            Transform projectileTransform = transform;
            projectileTransform.Position += transform.Forward * 5.0f;
            projectileDpa.Pose = transform;
        }
    }
}
