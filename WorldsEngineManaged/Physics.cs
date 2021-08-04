using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Explicit)]
    public struct RaycastHit
    {
        [FieldOffset(0)]
        public Entity HitEntity;
        [FieldOffset(4)]
        public Vector3 Normal;
        [FieldOffset(16)]
        public Vector3 WorldHitPos;
        [FieldOffset(28)]
        public float Distance;
    }

    public static class Physics
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_raycast(Vector3 origin, Vector3 direction, float maxDist, out RaycastHit hit);

        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDist = float.MaxValue)
        {
            return physics_raycast(origin, direction, maxDist, out RaycastHit _);
        }

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDist = float.MaxValue)
        {
            return physics_raycast(origin, direction, maxDist, out hit);
        }
    }
}
