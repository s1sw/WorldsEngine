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

    [Flags]
    public enum PhysicsLayers
    {
        None = 0,
        Default = 1,
        Player = 2,
        NoCollision = 4
    }

    public static class Physics
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_raycast(Vector3 origin, Vector3 direction, float maxDist, uint excludeLayerMask, out RaycastHit hit);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_overlapSphere(Vector3 origin, float radius, out uint overlappedEntity);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint physics_overlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, IntPtr entityBuffer);

        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDist = float.MaxValue, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
        {
            return physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out RaycastHit _);
        }

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDist = float.MaxValue, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
        {
            return physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out hit);
        }

        public static bool OverlapSphere(Vector3 origin, float radius, out Entity entity)
        {
            bool overlapped = physics_overlapSphere(origin, radius, out uint id);

            entity = new Entity(id);

            return overlapped;
        }

        public static uint OverlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Entity[] entityBuffer)
        {
            var handle = GCHandle.Alloc(entityBuffer, GCHandleType.Pinned);

            uint count = physics_overlapSphereMultiple(origin, radius, maxTouchCount, handle.AddrOfPinnedObject());

            handle.Free();

            return count;
        }
    }
}
