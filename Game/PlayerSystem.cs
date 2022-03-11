using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game
{
    class PlayerSystem : ISystem
    {
        public void OnSceneStart()
        {
            Camera.Main.Rotation = WorldsEngine.Math.Quaternion.Identity;
            Camera.Main.Position = WorldsEngine.Math.Vector3.Zero;
            if (Registry.View<SpawnPoint>().Count == 0) return;
            Entity spawnPointEntity = Registry.View<SpawnPoint>().GetEnumerator().Current;
            Transform spawnPoint = Registry.GetTransform(spawnPointEntity);
            //
            //Camera.Main.Position = spawnPoint.Position;

            if (Registry.View<PlayerRig>().Count > 0)
            {
                // Player exists, don't spawn!
                return;
            }

            Entity body = Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_body.wprefab"));
            Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_left_hand.wprefab"));
            Registry.CreatePrefab(AssetDB.PathToId("Prefabs/player_right_hand.wprefab"));
            spawnPoint.Scale = body.Transform.Scale;
            body.Transform = spawnPoint;
        }
    }
}
