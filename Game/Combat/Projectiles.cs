using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game.Combat
{
    public static class Projectiles
    {
        public static AssetID GetProjectilePrefab(AmmoType type) => type switch
        {
            AmmoType.Humongous => AssetDB.PathToId("Prefabs/big_ass_projectile.wprefab"),
            _ => AssetDB.PathToId("Prefabs/gun_projectile.wprefab"),
        };

        public static float GetProjectileSpeed(AmmoType type) => type switch
        {
            AmmoType.Humongous => 75f,
            _ => 100f
        };
    }
}
