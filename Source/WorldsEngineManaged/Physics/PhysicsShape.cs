using System;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public enum PhysicsShapeType : uint
    {
        Sphere,
        Box,
        Capsule,
        Mesh,
        ConvexMesh
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
        public Vector3 position = Vector3.Zero;
        public Quaternion rotation = Quaternion.Identity;
        public PhysicsShapeType type;
        public PhysicsMaterial? physicsMaterial;

        internal abstract PhysicsShapeInternal ToInternal();

        internal static PhysicsShape FromInternal(PhysicsShapeInternal psi)
        {
            PhysicsShape shape = psi.type switch
            {
                PhysicsShapeType.Sphere => new SpherePhysicsShape(psi.sphereRadius),
                PhysicsShapeType.Capsule => new CapsulePhysicsShape(psi.capsuleRadius, psi.capsuleHeight),
                PhysicsShapeType.Box => new BoxPhysicsShape(psi.boxHalfExtents),
                PhysicsShapeType.ConvexMesh => new ConvexMeshShape(psi.meshId),
                _ => new SpherePhysicsShape(0.5f),
            };

            shape.position = psi.position;
            shape.rotation = psi.rotation;
            if (psi.material != IntPtr.Zero)
            {
                shape.physicsMaterial = new PhysicsMaterial(psi.material);
            }

            return shape;
        }
    }

    public class BoxPhysicsShape : PhysicsShape
    {
        public Vector3 halfExtents;

        public BoxPhysicsShape(Vector3 halfExtents, PhysicsMaterial? material = null)
        {
            this.halfExtents = halfExtents;
            type = PhysicsShapeType.Box;
            physicsMaterial = material;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new()
            {
                type = PhysicsShapeType.Box,
                boxHalfExtents = halfExtents,
                position = position,
                rotation = rotation,
                material = physicsMaterial?.NativeHandle ?? IntPtr.Zero
            };

            return psi;
        }
    }

    public class SpherePhysicsShape : PhysicsShape
    {
        public float radius;

        public SpherePhysicsShape(float radius, PhysicsMaterial? material = null)
        {
            this.radius = radius;
            type = PhysicsShapeType.Sphere;
            physicsMaterial = material;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new()
            {
                type = PhysicsShapeType.Sphere,
                sphereRadius = radius,
                position = position,
                rotation = rotation,
                material = physicsMaterial?.NativeHandle ?? IntPtr.Zero
            };

            return psi;
        }
    }

    public class CapsulePhysicsShape : PhysicsShape
    {
        public float radius;
        public float height;

        public CapsulePhysicsShape(float radius, float height, PhysicsMaterial? material = null)
        {
            this.radius = radius;
            this.height = height;
            type = PhysicsShapeType.Capsule;
            physicsMaterial = material;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new()
            {
                type = PhysicsShapeType.Capsule,
                capsuleRadius = radius,
                capsuleHeight = height,
                material = physicsMaterial?.NativeHandle ?? IntPtr.Zero
            };

            return psi;
        }
    }

    public class ConvexMeshShape : PhysicsShape
    {
        public AssetID MeshID;

        public ConvexMeshShape(AssetID meshId, PhysicsMaterial? material = null)
        {
            MeshID = meshId;
            type = PhysicsShapeType.ConvexMesh;
            physicsMaterial = material;
        }

        internal override PhysicsShapeInternal ToInternal()
        {
            PhysicsShapeInternal psi = new()
            {
                type = PhysicsShapeType.ConvexMesh,
                meshId = MeshID,
                material = physicsMaterial?.NativeHandle ?? IntPtr.Zero
            };

            return psi;
        }
    }
}
