using JetBrains.Annotations;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;
using System.Reflection;

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
        [FieldOffset(32)]
        public uint HitLayer;
    }

    public enum PhysicsLayer
    {
        Default = 0,
        Player = 1,
        NoCollision = 2
    }

    [Flags]
    public enum PhysicsLayerMask
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
        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDist = float.MaxValue, PhysicsLayerMask excludeLayerMask = PhysicsLayerMask.None)
            => physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out RaycastHit _);

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDist = float.MaxValue, PhysicsLayerMask excludeLayerMask = PhysicsLayerMask.None)
            => physics_raycast(origin, direction, maxDist, (uint)excludeLayerMask, out hit);

        public static bool OverlapSphere(Vector3 origin, float radius, out Entity entity)
        {
            bool overlapped = physics_overlapSphere(origin, radius, out uint id);

            entity = new Entity(id);

            return overlapped;
        }

        public static unsafe uint OverlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Span<Entity> entityBuffer, PhysicsLayerMask excludeLayerMask = PhysicsLayerMask.None)
        {
            uint count;

            fixed (Entity* ptr = entityBuffer)
            {
                count = physics_overlapSphereMultiple(origin, radius, maxTouchCount, ptr, (uint)excludeLayerMask);
            }

            return count;
        }

        public static bool SweepSphere(Vector3 origin, float radius, Vector3 direction, float distance, out RaycastHit hit, PhysicsLayerMask excludeLayerMask = PhysicsLayerMask.None)
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
            try
            {
                _callback(pairArray);
            }
            catch (Exception e)
            {
                Log.Error($"Caught exception in contact mod callback! {e}");
            }
        }

        struct OldCallback
        {
            public string TypeName;
            public string MethodName;
            public BindingFlags Flags;
        }

        private static OldCallback? _lastContactModCallback;

        private static BindingFlags GetBindingFlags(MethodInfo info)
        {
            BindingFlags flags = BindingFlags.Default;

            flags |= info.IsStatic ? BindingFlags.Static : BindingFlags.Instance;
            flags |= info.IsPublic ? BindingFlags.Public : BindingFlags.NonPublic;

            return flags;
        }

        static Physics()
        {
            Engine.AssemblyLoadManager.OnAssemblyUnload += () =>
            {
                if (_callback == null) return;

                MethodInfo callbackMethod = _callback.Method;
                _lastContactModCallback = new OldCallback()
                {
                    TypeName = callbackMethod.DeclaringType!.FullName!,
                    MethodName = callbackMethod.Name,
                    Flags = GetBindingFlags(callbackMethod)
                };
                _callback = null;
            };

            Engine.AssemblyLoadManager.OnAssemblyLoad += (Assembly asm) =>
            { 
                Log.Msg("Physics OnAssemblyLoad");
                if (_lastContactModCallback == null) return;
                Log.Msg("callback wasn't null");

                OldCallback oc = _lastContactModCallback.Value;
                var type = asm.GetType(oc.TypeName);

                if (type == null)
                {
                    Log.Error("Failed to set contact mod callback: type not found");
                    return;
                }

                MethodInfo? method = type.GetMethod(oc.MethodName, oc.Flags);

                if (method == null)
                {
                    Log.Error("Failed to set contact mod callback: method not found");
                    return;
                }

                _callback = (ContactModCallback)Delegate.CreateDelegate(typeof(ContactModCallback), method);
            };
        }
    }
}
