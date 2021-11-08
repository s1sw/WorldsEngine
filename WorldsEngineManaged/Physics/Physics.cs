using JetBrains.Annotations;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
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

        struct Collision
        {
            public uint EntityID;
            public PhysicsContactInfo ContactInfo;
        }

        private static readonly Queue<Collision> _collisionQueue = new();

        [UsedImplicitly]
        [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
            Justification = "Called from native C++ during deserialization")]
        private static void HandleCollisionFromNative(uint entityId, ref PhysicsContactInfo contactInfo)
        {
            _collisionQueue.Enqueue(new Collision() { EntityID = entityId, ContactInfo = contactInfo });
        }

        internal static void FlushCollisionQueue()
        {
            while (_collisionQueue.Count > 0)
            {
                var collision = _collisionQueue.Dequeue();
                try
                {
                    if (Registry.Valid(new Entity(collision.EntityID)))
                    {
                        Registry.HandleCollision(collision.EntityID, ref collision.ContactInfo);
                    }
                }
                catch (Exception e)
                {
                    Logger.LogError($"Caught exception: {e}");
                }
            }
        }

        internal static void ClearCollisionQueue()
        {
            _collisionQueue.Clear();
        }
    }
}
