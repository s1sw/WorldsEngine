using System;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Sequential)]
    struct Vector3
    {
        public float x, y, z;

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
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

        public static Vector3 operator*(Vector3 v, Quaternion q)
        {
            Vector3 u = new Vector3(q.x, q.y, q.z);
            float s = q.w;

            return 2.0f * u.Dot(v) * u
                + (s*s - u.LengthSquared) * v
                + 2.0f * s * u.Cross(v);
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
    }
}
