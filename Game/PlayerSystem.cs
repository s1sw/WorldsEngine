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
            //Entity spawnPointEntity = Registry.View<SpawnPoint>().GetEnumerator().Current;
            //Transform spawnPoint = Registry.GetTransform(spawnPointEntity);
            //
            //Camera.Main.Position = spawnPoint.Position;
        }
    }
}
