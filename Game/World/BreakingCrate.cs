using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;

namespace Game.World;

[Component]
public class BreakingCrate : Component, ICollisionHandler
{
    private static AssetID TopPrefab = AssetDB.PathToId("Prefabs/crate_top.wprefab");
    private static AssetID BottomPrefab = AssetDB.PathToId("Prefabs/crate_bottom.wprefab");

    private bool _broken = false;

    public void OnCollision(ref PhysicsContactInfo contactInfo)
    {
        if (_broken) return;

        if (contactInfo.RelativeSpeed > 15.0f)
        {
            _broken = true;
            Entity top = Registry.CreatePrefab(TopPrefab);
            Entity bottom = Registry.CreatePrefab(BottomPrefab);

            Transform t = Entity.Transform;
            t.Position += WorldsEngine.Math.Vector3.Up * 0.2f;
            top.Transform = t;
            bottom.Transform = Entity.Transform;

            Vector3 forceVec = new();

            forceVec.x += System.Random.Shared.NextSingle() * 2.0f - 1.0f;
            forceVec.z += System.Random.Shared.NextSingle() * 2.0f - 1.0f;
            forceVec *= 0.1f;
            forceVec.y = 1.0f;
            forceVec.Normalize();

            forceVec = Entity.Transform.TransformDirection(forceVec);

            if (System.Random.Shared.NextSingle() > 0.0f)
            {
                Entity pickup = Registry.CreatePrefab(AssetDB.PathToId("Prefabs/scrap_metal_pickup.wprefab"));
                t.Scale = Vector3.One;
                t.Position -= Vector3.Up * 0.1f;
                pickup.Transform = t;
            }

            top.GetComponent<DynamicPhysicsActor>().AddForce(forceVec * 15.0f, ForceMode.Impulse);
            bottom.GetComponent<DynamicPhysicsActor>().AddForce(-forceVec * 15.0f, ForceMode.Impulse);

            Registry.DestroyNext(Entity);
            Audio.PlayOneShotEvent("event:/Misc/Crate open", top.Transform.Position);
        }
    }
}