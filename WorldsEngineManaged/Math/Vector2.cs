using System;
using System.Runtime.InteropServices;

namespace WorldsEngine.Math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public static Vector2 Zero => new Vector2(0.0f, 0.0f);

        public float Length => MathF.Sqrt(LengthSquared);
        public float LengthSquared => (x * x) + (y * y);
        public Vector2 Normalized
        {
            get
            {
                if (LengthSquared == 0.0f) return Zero;
                return this / Length;
            }
        }

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

        public static Vector2 operator/(Vector2 v, float divisor)
        {
            return new Vector2(v.x / divisor, v.y / divisor);
        }

        public static float Dot(Vector2 a, Vector2 b)
        {
            return (a.x * b.x) + (a.y * b.y);
        }

        public float Dot(Vector2 other)
        {
            return Dot(this, other);
        }

        public void Normalize()
        {
            if (LengthSquared == 0.0f) return;
            this /= Length;
        }
    }
}
