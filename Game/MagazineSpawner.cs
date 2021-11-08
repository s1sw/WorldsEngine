using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game
{
    [Component]
    public class MagazineSpawner
    {
        public AmmoType AmmoType = AmmoType.Laser;
        [JsonIgnore]
        public Entity CurrentMagazine = Entity.Null;
        [JsonIgnore]
        public bool SpawningMagazine = false;
    }

    public class MagazineSpawnerSystem : ISystem
    {
        private Entity SpawnMagazineFor(MagazineSpawner spawner, Transform spawnTransform)
        {
            AssetID magazinePrefab = spawner.AmmoType switch
            {
                AmmoType.Humongous => AssetDB.PathToId("Prefabs/Ammo/humongous.wprefab"),
                AmmoType.Laser or _ => AssetDB.PathToId("Prefabs/Ammo/laser.wprefab")
            };

            Entity spawned = Registry.CreatePrefab(magazinePrefab);

            // Now set the new position and rotation to that of the spawner...
            var magazineDpa = Registry.GetComponent<DynamicPhysicsActor>(spawned);
            magazineDpa.Pose = spawnTransform;

            // and add a D6 joint to keep it in place.
            var d6 = Registry.AddComponent<D6Joint>(spawned);
            d6.SetAllAxisMotion(D6Motion.Locked);
            d6.TargetLocalPose = spawnTransform;
            d6.BreakForce = 50f;

            return spawned;
        }

        private async void SpawnNewMagazine(Entity entity)
        {
            MagazineSpawner spawner = Registry.GetComponent<MagazineSpawner>(entity);
            spawner.SpawningMagazine = true;
            await Task.Delay(2000);
            spawner.CurrentMagazine = SpawnMagazineFor(spawner, Registry.GetTransform(entity));
            spawner.SpawningMagazine = false;
        }

        public void OnUpdate()
        {
            var spawners = Registry.View<MagazineSpawner>();

            foreach (var entity in spawners)
            {
                MagazineSpawner spawner = Registry.GetComponent<MagazineSpawner>(entity);

                if (spawner.CurrentMagazine.IsNull && !spawner.SpawningMagazine)
                {
                    SpawnNewMagazine(entity);
                }

                if (!spawner.CurrentMagazine.IsNull)
                {
                    var magd6 = Registry.GetComponent<D6Joint>(spawner.CurrentMagazine);
                    if (magd6.IsBroken)
                    {
                        Registry.RemoveComponent<D6Joint>(spawner.CurrentMagazine);
                        spawner.CurrentMagazine = Entity.Null;
                    }
                }
            }
        }
    }
}
