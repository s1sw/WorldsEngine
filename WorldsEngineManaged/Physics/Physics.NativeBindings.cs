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

        public System.Collections.Generic.IEnumerator<ContactModifyPair> GetEnumerator()
        {
            for (int i = 0; i < Count; i++)
            {
                yield return this[i];
            }
        }
    }

    // TODO: Maybe we can use this at some point instead of making an unmanaged call for everything?
    [StructLayout(LayoutKind.Sequential, Pack = 16)]
    internal struct PxModifiableContact
    {
        public Vector3 Contact;
        public float Separation;
        public Vector3 TargetVelocity;
        public float MaxImpulse;
        public Vector3 Normal;
        public float Restitution;
        public uint MaterialFlags;
        public ushort MaterialIndex0;
        public ushort MaterialIndex1;
        public float StaticFriction;
        public float DynamicFriction;
    }

    public class ModifiableContact
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_getTargetVelocity(IntPtr ptr, int idx, out Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setTargetVelocity(IntPtr ptr, int idx, Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_getNormal(IntPtr ptr, int idx, out Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setNormal(IntPtr ptr, int idx, Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float ContactSet_getMaxImpulse(IntPtr ptr, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setMaxImpulse(IntPtr ptr, int idx, float impulse);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float ContactSet_getDynamicFriction(IntPtr ptr, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setDynamicFriction(IntPtr ptr, int idx, float val);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float ContactSet_getStaticFriction(IntPtr ptr, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setStaticFriction(IntPtr ptr, int idx, float val);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float ContactSet_getRestitution(IntPtr ptr, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setRestitution(IntPtr ptr, int idx, float val);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float ContactSet_getSeparation(IntPtr ptr, int idx);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_setSeparation(IntPtr ptr, int idx, float val);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void ContactSet_getPoint(IntPtr ptr, int idx, out Vector3 value);

        private readonly IntPtr _contactSetPtr;
        internal int idx;

        public Vector3 TargetVelocity
        {
            get
            {
                Vector3 v = new();
                ContactSet_getTargetVelocity(_contactSetPtr, idx, out v);
                return v;
            }
            set => ContactSet_setTargetVelocity(_contactSetPtr, idx, value);
        }

        public Vector3 Normal
        {
            get
            {
                Vector3 v = new();
                ContactSet_getNormal(_contactSetPtr, idx, out v);
                return v;
            }

            set => ContactSet_setNormal(_contactSetPtr, idx, value);
        }

        public float MaxImpulse
        {
            get => ContactSet_getMaxImpulse(_contactSetPtr, idx);
            set => ContactSet_setMaxImpulse(_contactSetPtr, idx, value);
        }

        public float DynamicFriction
        {
            get => ContactSet_getDynamicFriction(_contactSetPtr, idx);
            set => ContactSet_setDynamicFriction(_contactSetPtr, idx, value);
        }

        public float StaticFriction
        {
            get => ContactSet_getStaticFriction(_contactSetPtr, idx);
            set => ContactSet_setStaticFriction(_contactSetPtr, idx, value);
        }

        public float Restitution
        {
            get => ContactSet_getRestitution(_contactSetPtr, idx);
            set => ContactSet_setRestitution(_contactSetPtr, idx, value);
        }

        public float Separation
        {
            get => ContactSet_getSeparation(_contactSetPtr, idx);
            set => ContactSet_setSeparation(_contactSetPtr, idx, value);
        }

        public Vector3 Point
        {
            get
            {
                Vector3 v = new();
                ContactSet_getPoint(_contactSetPtr, idx, out v);
                return v;
            }
        }

        internal ModifiableContact(IntPtr contactSetPtr, int idx)
        {
            _contactSetPtr = contactSetPtr;
            this.idx = idx;
        }
    }

    public class ContactSet
    {
        private IntPtr _nativePtr;
        private ModifiableContact _contactDummy;

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint ContactSet_getCount(IntPtr ptr);

        public int Count => (int)ContactSet_getCount(_nativePtr);

        public ModifiableContact this[int index]
        {
            get
            {
                _contactDummy.idx = index;
                return _contactDummy;
            }
        }

        internal ContactSet(IntPtr ptr)
        {
            _contactDummy = new(ptr, 0);
            _nativePtr = ptr;
        }

        public System.Collections.Generic.IEnumerator<ModifiableContact> GetEnumerator()
        {
            for (int i = 0; i < Count; i++)
            {
                yield return new ModifiableContact(_nativePtr, i);
            }
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

        public bool InvolvesEntity(Entity entity)
        {
            return EntityA == entity || EntityB == entity;
        }

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
        private static extern void physics_setContactModCallback(IntPtr ctx, NativeContactModCallback? callback);
    }
}
