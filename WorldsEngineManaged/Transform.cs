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

        public Vector3 TransformPoint(Vector3 point)
        {
            return Position + (Rotation * point);
        }

        public Vector3 InverseTransformPoint(Vector3 point)
        {
            return Rotation.Inverse * (point - Position);
        }

        public Vector3 TransformDirection(Vector3 direction)
        {
            return Rotation * direction;
        }
    }
}
