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

    public struct PhysicsContactInfo
    {
        public float RelativeSpeed;
        public Entity OtherEntity;
        public Vector3 AverageContactPoint;
        public Vector3 Normal;
    };

    public struct CollisionHandlerHandle
    {
        internal uint ID;
    }

    public static class Physics
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_raycast(Vector3 origin, Vector3 direction, float maxDist, uint excludeLayerMask, out RaycastHit hit);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_overlapSphere(Vector3 origin, float radius, out uint overlappedEntity);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint physics_overlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, IntPtr entityBuffer, uint excludeLayerMask);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NativeCollisionDelegate(uint entityId, uint id, ref PhysicsContactInfo contactInfo);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void physics_addEventHandler(NativeCollisionDelegate collisionDelegate);

        private static Dictionary<uint, ICollisionHandler> _collisionHandlers = new();
        private static uint _id = 0;

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

        public static uint OverlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Entity[] entityBuffer, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
        {
            var handle = GCHandle.Alloc(entityBuffer, GCHandleType.Pinned);

            uint count = physics_overlapSphereMultiple(origin, radius, maxTouchCount, handle.AddrOfPinnedObject(), (uint)excludeLayerMask);

            handle.Free();

            return count;
        }

        public static CollisionHandlerHandle RegisterCollisionHandler(ICollisionHandler handler, Entity entity)
        {
            uint handle = _id;
            _collisionHandlers.Add(handle, handler);
            // native call

            _id++;

            return new CollisionHandlerHandle() { ID = handle };
        }

        private static void HandleCollision(uint entityId, uint collisionHandlerId, ref PhysicsContactInfo contactInfo)
        {
            _collisionHandlers[collisionHandlerId].OnCollision(new Entity(entityId), ref contactInfo);
        }
    }
}
