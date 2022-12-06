using System;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Position;
        public Quaternion Rotation;
        public Vector3 Scale;

        public Vector3 Forward => Rotation * Vector3.Forward;
        public Vector3 Backward => Rotation * Vector3.Backward;
        public Vector3 Right => Rotation * Vector3.Right;
        public Vector3 Left => Rotation * Vector3.Left;
        public Vector3 Up => Rotation * Vector3.Up;
        public Vector3 Down => Rotation * Vector3.Down;

        public Transform(Vector3 position, Quaternion rotation)
        {
            Position = position;
            Rotation = rotation;
            Scale = new Vector3(1.0f);
        }

        public Transform TransformBy(Transform other)
        {
            return new Transform(other.Position + (other.Rotation * Position), other.Rotation * Rotation);
        }

        public Transform TransformByInverse(Transform other)
        {
            return new Transform(other.Rotation.Inverse * (Position - other.Position), other.Rotation.Inverse * Rotation);
        }

        public Vector3 TransformPoint(Vector3 point) => Position + (Rotation * point);
        public Vector3 InverseTransformPoint(Vector3 point) => Rotation.Inverse * (point - Position);
        public Vector3 TransformDirection(Vector3 direction) => Rotation * direction;
        public Vector3 InverseTransformDirection(Vector3 normal) => Rotation.Inverse * normal;
    }
}
