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

    public readonly struct PhysicsContactInfo
    {
        public readonly float RelativeSpeed;
        public readonly Entity OtherEntity;
        public readonly Vector3 AverageContactPoint;
        public readonly Vector3 Normal;
    }

    public delegate void ContactModCallback(ContactModPairArray pairs);

    public static partial class Physics
    {
        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDist = float.MaxValue, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
            => physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out RaycastHit _);

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDist = float.MaxValue, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
            => physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out hit);

        public static bool OverlapSphere(Vector3 origin, float radius, out Entity entity)
        {
            bool overlapped = physics_overlapSphere(origin, radius, out uint id);

            entity = new Entity(id);

            return overlapped;
        }

        public static unsafe uint OverlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Span<Entity> entityBuffer, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
        {
            uint count;

            fixed (Entity* ptr = entityBuffer)
            {
                count = physics_overlapSphereMultiple(origin, radius, maxTouchCount, ptr, (uint)excludeLayerMask);
            }

            return count;
        }

        public static bool SweepSphere(Vector3 origin, float radius, Vector3 direction, float distance, out RaycastHit hit, PhysicsLayers excludeLayerMask = PhysicsLayers.None)
            => physics_sweepSphere(origin, radius, direction, distance, out hit, (uint)excludeLayerMask);


        public static ContactModCallback? ContactModCallback
        {
            set
            {
                if (value == null)
                {
                    _callback = null;
                    physics_setContactModCallback(IntPtr.Zero, null);
                    return;
                }

                _callback = value;
                physics_setContactModCallback(IntPtr.Zero, _nativeContactModCallback);
            }
        }
        
        private static ContactModCallback? _callback;
        private static NativeContactModCallback _nativeContactModCallback = new(CallbackWrapper);

        private static void CallbackWrapper(IntPtr ctx, IntPtr pairs, uint count)
        {
            if (_callback == null) return;

            ContactModPairArray pairArray = new(pairs, count);
            _callback(pairArray);
        }
    }
}
