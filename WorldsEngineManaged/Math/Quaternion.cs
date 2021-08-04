using System;
using System.Runtime.InteropServices;

namespace WorldsEngine.Math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float x, y, z, w;

        public static Quaternion Identity => new Quaternion(1.0f, 0.0f, 0.0f, 0.0f);

        public float LengthSquared => x * x + y * y + z * z + w * w;
        public float Length => MathF.Sqrt(LengthSquared);

        public Quaternion Conjugate => new Quaternion(w, -x, -y, -z);
        public Quaternion Inverse => Conjugate / LengthSquared;

        public Vector3 Axis
        {
            get
            {
                float a = MathF.Sqrt(1 - (w * w));

                // Angle of 0 degrees so the axis doesn't matter
                // Return an arbitrary unit vector
                if (a < 0.001f)
                {
                    return Vector3.Left;
                }

                return new Vector3(x / a, y / a, z / a);
            }
        }

        public float Angle => 2f * MathF.Acos(MathF.Min(MathF.Max(w, -1.0f), 1.0f));

        public bool HasNaNComponent => float.IsNaN(x) || float.IsNaN(y) || float.IsNaN(z) || float.IsNaN(w);

        public Quaternion(float w, float x, float y, float z)
        {
            this.w = w;
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public static float Dot(Quaternion left, Quaternion right)
        {
            return (left.x * right.x) + (left.y * right.y) + (left.z * right.z) * (left.w * right.w);
        }

        public static Quaternion AngleAxis(float angle, Vector3 axis)
        {
            return new Quaternion(
                MathF.Cos(angle * 0.5f),
                axis.x * MathF.Sin(angle * 0.5f),
                axis.y * MathF.Sin(angle * 0.5f),
                axis.z * MathF.Sin(angle * 0.5f)
            );
        }

        public static Quaternion LookAt(Vector3 direction, Vector3 up)
        {
            direction.Normalize();

            Vector3 column0 = up.Cross(direction).Normalized;
            Vector3 column1 = direction.Cross(column0);
            Vector3 column2 = direction;

            return (Quaternion)new Mat3x3(column0, column1, column2);
        }

        public static Quaternion SafeLookAt(Vector3 dir)
        {
            return SafeLookAt(dir, Vector3.Up, Vector3.Forward);
        }

        public static Quaternion SafeLookAt(Vector3 dir, Vector3 up, Vector3 fallbackUp)
        {
            return MathF.Abs(dir.Dot(up)) > 0.999f ? LookAt(dir, fallbackUp) : LookAt(dir, up);
        }

        public static Vector3 operator *(Quaternion q, Vector3 v)
        {
            Vector3 QuatVector = new Vector3(q.x, q.y, q.z);
            Vector3 uv = Vector3.Cross(QuatVector, v);
            Vector3 uuv = Vector3.Cross(QuatVector, uv);

            return v + ((uv * q.w) + uuv) * 2.0f;
        }

        public static Quaternion operator*(Quaternion a, Quaternion b)
        {
            return new Quaternion(
                -a.x * b.x - a.y * b.y - a.z * b.z + a.w * b.w,
                a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x,
                -a.x * b.z + a.y * b.w + a.z * b.x + a.w * b.y,
                a.x * b.y - a.y * b.x + a.z * b.w + a.w * b.z
             );
        }

        public static Quaternion operator/(Quaternion q, float scalar)
        {
            return new Quaternion(q.w / scalar, q.x / scalar, q.y / scalar, q.z / scalar);
        }
    }
}
