using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

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
        public PhysicsShapeType type;

        internal abstract PhysicsShapeInternal ToInternal();

        internal static PhysicsShape FromInternal(PhysicsShapeInternal psi)
        {
            PhysicsShape shape;
            switch (psi.type)
            {
                case PhysicsShapeType.Sphere:
                    shape = new SpherePhysicsShape(psi.sphereRadius);
                    break;
                case PhysicsShapeType.Capsule:
                    shape = new CapsulePhysicsShape(psi.capsuleRadius, psi.capsuleHeight);
                    break;
                case PhysicsShapeType.Box:
                    shape = new BoxPhysicsShape(psi.boxHalfExtents);
                    break;
                default:
                    shape = new SpherePhysicsShape(0.5f);
                    break;
            }

            shape.position = psi.position;
            shape.rotation = psi.rotation;

            return shape;
        }
    }

    public class BoxPhysicsShape : PhysicsShape
    {
        public Vector3 halfExtents;

        public BoxPhysicsShape(Vector3 halfExtents)
        {
            this.halfExtents = halfExtents;
            type = PhysicsShapeType.Box;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal
            {
                type = PhysicsShapeType.Box,
                boxHalfExtents = halfExtents,
                position = position,
                rotation = rotation
            };

            return psi;
        }
    }

    public class SpherePhysicsShape : PhysicsShape
    {
        public float radius;

        public SpherePhysicsShape(float radius)
        {
            this.radius = radius;
            type = PhysicsShapeType.Sphere;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal
            {
                type = PhysicsShapeType.Sphere,
                sphereRadius = radius,
                position = position,
                rotation = rotation
            };

            return psi;
        }
    }

    public class CapsulePhysicsShape : PhysicsShape
    {
        public float radius;
        public float height;

        public CapsulePhysicsShape(float radius, float height)
        {
            this.radius = radius;
            this.height = height;
            type = PhysicsShapeType.Capsule;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new PhysicsShapeInternal
            {
                type = PhysicsShapeType.Capsule,
                capsuleRadius = radius,
                capsuleHeight = height
            };

            return psi;
        }
    }
}
