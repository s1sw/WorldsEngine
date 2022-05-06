using WorldsEngine;
using WorldsEngine.Math;
using Game.Combat;
using System.Collections.Generic;
using Game.World;

namespace Game;

[Component]
public class ExplodeOnDeath : Component, IStartListener
{
    public void Start()
    {
        var hc = Registry.GetComponent<HealthComponent>(Entity);
        hc.OnDeath += OnDeath;
    }

    private Entity SpawnCube(Transform transform)
    {
        var entity = Registry.Create();
        Registry.SetTransform(entity, transform);

        var worldObject = Registry.AddComponent<WorldObject>(entity);
        worldObject.Mesh = AssetDB.PathToId("Models/cube.wmdl");

        var dpa = Registry.AddComponent<DynamicPhysicsActor>(entity);
        dpa.Mass = 0.25f;

        var physSounds = Registry.AddComponent<CollisionSound>(entity);
        physSounds.EventPath = "event:/Impacts/ReallyLight";

        List<PhysicsShape> physicsShapes = new()
        {
            new BoxPhysicsShape(Vector3.One)
        };
        dpa.SetPhysicsShapes(physicsShapes);

        return entity;
    }

    private void OnDeath(Entity e)
    {
        var transform = Registry.GetTransform(e);
        transform.Scale = new Vector3(0.1f);

        async static void DestroyAfter(int ms, Entity entity)
        {
            await System.Threading.Tasks.Task.Delay(ms);
            if (Registry.Valid(entity))
                Registry.Destroy(entity);
        }

        for (int i = 0; i < 100; i++)
        {
            var spawnTransform = transform;
            var pos = spawnTransform.Position;
            pos += new Vector3(0.0f, 1f + (i * 0.15f), 0.0f);
            spawnTransform.Position = pos;
            DestroyAfter(7000, SpawnCube(spawnTransform));
        }

        Registry.Destroy(e);
    }
}