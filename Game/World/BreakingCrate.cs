using WorldsEngine;
using WorldsEngine.Audio;

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

            top.Transform = Entity.Transform;
            bottom.Transform = Entity.Transform;

            top.GetComponent<DynamicPhysicsActor>().AddForce(WorldsEngine.Math.Vector3.Up * 15.0f, ForceMode.Impulse);
            bottom.GetComponent<DynamicPhysicsActor>().AddForce(WorldsEngine.Math.Vector3.Down * 15.0f, ForceMode.Impulse);

            Registry.DestroyNext(Entity);
            Audio.PlayOneShotEvent("event:/Misc/Crate open", top.Transform.Position);
        }
    }
}