using System;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public enum PhysicsShapeType : uint
    {
        Sphere,
        Box,
        Capsule,
        Mesh
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct PhysicsShapeInternal
    {
        [FieldOffset(0)]
        public PhysicsShapeType type;

        [FieldOffset(4)]
        public Vector3 boxHalfExtents;

        [FieldOffset(4)]
        public float sphereRadius;

        [FieldOffset(4)]
        public float capsuleHeight;
        [FieldOffset(8)]
        public float capsuleRadius;

        [FieldOffset(4)]
        public AssetID meshId;

        [FieldOffset(16)]
        public Vector3 position;

        [FieldOffset(28)]
        public Quaternion rotation;

        [FieldOffset(48)]
        public IntPtr material;
    }

    public abstract class PhysicsShape
    {
        public Vector3 position;
        public Quaternion rotation;

        internal abstract PhysicsShapeInternal ToInternal();
    }

    public class BoxPhysicsShape : PhysicsShape
    {
        public Vector3 halfExtents;

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal();

            psi.type = PhysicsShapeType.Box;
            psi.boxHalfExtents = halfExtents;
            psi.position = position;
            psi.rotation = rotation;

            return psi;
        }
    }

    public class SpherePhysicsShape : PhysicsShape
    {
        public float radius;

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal();

            psi.type = PhysicsShapeType.Sphere;
            psi.sphereRadius = radius;
            psi.position = position;
            psi.rotation = rotation;

            return psi;
        }
    }

    public class CapsulePhysicsShape : PhysicsShape
    {
        public float radius;
        public float height;

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal();

            psi.type = PhysicsShapeType.Capsule;
            psi.capsuleRadius = radius;
            psi.capsuleHeight = height;

            return psi;
        }
    }
}
