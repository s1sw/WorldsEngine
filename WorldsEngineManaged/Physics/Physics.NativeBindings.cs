using System;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public class ContactModPairArray
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern int ContactModifyPair_getSize();

        public readonly uint Count;

        private readonly IntPtr _nativePtr;
        private readonly int _stride;

        internal ContactModPairArray(IntPtr nativePtr, uint count)
        {
            _nativePtr = nativePtr;
            Count = count;
            _stride = ContactModifyPair_getSize();
        }

        public ContactModifyPair this[int index]
        {
            get
            {
                return new ContactModifyPair(IntPtr.Add(_nativePtr, index * _stride));
            }
        }
    }

    public class ContactSet
    {
        private IntPtr _nativePtr;
        
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_getTargetVelocity(IntPtr ptr, int idx, out Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setTargetVelocity(IntPtr ptr, int idx, Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_getNormal(IntPtr ptr, int idx, out Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setNormal(IntPtr ptr, int idx, Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint ContactSet_getCount(IntPtr ptr);

        public int Count => (int)ContactSet_getCount(_nativePtr);

        public Vector3 GetTargetVelocity(int i)
        {
            Vector3 v = new();
            ContactSet_getTargetVelocity(_nativePtr, i, out v);
            return v;
        }
        
        public void SetTargetVelocity(int i, Vector3 v)
        {
            ContactSet_setTargetVelocity(_nativePtr, i, v);
        }

        internal ContactSet(IntPtr ptr)
        {
            _nativePtr = ptr;
        }
    }

    public struct ContactModifyPair
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint ContactModifyPair_getEntity(IntPtr pair, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactModifyPair_getTransform(IntPtr pair, int idx, out Transform t);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern IntPtr ContactModifyPair_getContactSetPointer(IntPtr pair);

        private IntPtr _nativePtr;

        public Entity EntityA => new Entity(ContactModifyPair_getEntity(_nativePtr, 0));
        public Entity EntityB => new Entity(ContactModifyPair_getEntity(_nativePtr, 1));

        public Transform TransformA
        {
            get
            {
                Transform t = new();
                ContactModifyPair_getTransform(_nativePtr, 0, out t);
                return t;
            }
        }

        public Transform TransformB
        {
            get
            {
                Transform t = new();
                ContactModifyPair_getTransform(_nativePtr, 1, out t);
                return t;
            }
        }

        public ContactSet ContactSet => _contactSet;

        private ContactSet _contactSet;

        internal ContactModifyPair(IntPtr nativePtr)
        {
            _nativePtr = nativePtr;
            _contactSet = new ContactSet(ContactModifyPair_getContactSetPointer(nativePtr));
        }
    }

    public static partial class Physics
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_raycast(Vector3 origin, Vector3 direction, float maxDist, uint excludeLayerMask, out RaycastHit hit);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_overlapSphere(Vector3 origin, float radius, out uint overlappedEntity);

        [DllImport(WorldsEngine.NativeModule)]
        private static unsafe extern uint physics_overlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Entity* entityBuffer, uint excludeLayerMask);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static unsafe extern bool physics_sweepSphere(Vector3 origin, float radius, Vector3 direction, float distance, out RaycastHit hit, uint excludeLayerMask);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NativeCollisionDelegate(uint entityId, uint id, ref PhysicsContactInfo contactInfo);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void physics_addEventHandler(NativeCollisionDelegate collisionDelegate);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NativeContactModCallback(IntPtr ctx, IntPtr pairs, uint count);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void physics_setContactModCallback(IntPtr ctx, NativeContactModCallback callback);
    }
}
