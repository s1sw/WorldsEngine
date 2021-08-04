using System;
using System.Runtime.InteropServices;

namespace WorldsEngine.Math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float x, y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static Vector2 operator+(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x + b.x, a.y + b.y);
        }

        public static Vector2 operator-(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x - b.x, a.y - b.y);
        }

        public static Vector2 operator*(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x * b.x, a.y * b.y);
        }

        public static Vector2 operator*(Vector2 v, float scalar)
        {
            return new Vector2(v.x * scalar, v.y * scalar);
        }

        public static Vector2 operator*(float scalar, Vector2 v)
        {
            return new Vector2(v.x * scalar, v.y * scalar);
        }

        public static float Dot(Vector2 a, Vector2 b)
        {
            return (a.x * b.x) + (a.y * b.y);
        }

        public float Length => MathF.Sqrt(LengthSquared);

        public float LengthSquared => (x * x) + (y * y);

        public float Dot(Vector2 other)
        {
            return Dot(this, other);
        }
    }
}
