using System;
using System.Runtime.InteropServices;

namespace WorldsEngine.Math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public static readonly Vector3 Zero = new Vector3(0.0f, 0.0f, 0.0f);
        public static readonly Vector3 One = new Vector3(1.0f);

        public static readonly Vector3 Forward = new Vector3(0.0f, 0.0f, 1.0f);
        public static readonly Vector3 Backward = new Vector3(0.0f, 0.0f, -1.0f);

        public static readonly Vector3 Left = new Vector3(1.0f, 0.0f, 0.0f);
        public static readonly Vector3 Right = new Vector3(-1.0f, 0.0f, 0.0f);

        public static readonly Vector3 Up = new Vector3(0.0f, 1.0f, 0.0f);
        public static readonly Vector3 Down = new Vector3(0.0f, -1.0f, 0.0f);

        public float x, y, z;

        public float ComponentSum => x + y + z;
        public float ComponentMean => (x + y + z) / 3.0f;

        public bool HasNaNComponent => float.IsNaN(x) || float.IsNaN(y) || float.IsNaN(z);
        public bool IsZero => x == 0.0f && y == 0.0f && z == 0.0f;
        public Vector3 Normalized => this / Length;

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vector3(float value)
        {
            x = value;
            y = value;
            z = value;
        }

        public static Vector3 operator-(Vector3 v)
        {
            return new Vector3(-v.x, -v.y, -v.z);
        }

        public static Vector3 operator+(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        }

        public static Vector3 operator-(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        }

        public static Vector3 operator*(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
        }

        public static Vector3 operator*(Vector3 v, float scalar)
        {
            return new Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
        }

        public static Vector3 operator*(float scalar, Vector3 v)
        {
            return new Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
        }

        public static Vector3 operator/(Vector3 v, float scalar)
        {
            return new Vector3(v.x / scalar, v.y / scalar, v.z / scalar);
        }

        public static float Dot(Vector3 a, Vector3 b)
        {
            return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
        }

        public static Vector3 Cross(Vector3 a, Vector3 b)
        {
            return new Vector3(
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
        }

        public float Length => MathF.Sqrt(LengthSquared);

        public float LengthSquared => (x * x) + (y * y) + (z * z);

        public float Dot(Vector3 other)
        {
            return Dot(this, other);
        }

        public Vector3 Cross(Vector3 other)
        {
            return Cross(this, other);
        }

        public Vector3 ClampMagnitude(float maxMagnitude)
        {
            return (this / Length) * MathF.Min(MathF.Max(-maxMagnitude, Length), maxMagnitude);
        }

        public void Normalize()
        {
            float len = Length;
            if (len == 0.0f) return;
            x /= len;
            y /= len;
            z /= len;
        }

        public float DistanceTo(Vector3 other)
        {
            return (this - other).Length;
        }

        public override string ToString()
        {
            return $"({x:0.###}, {y:0.###}, {z:0.###})";
        }
    }
}
