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
        public Quaternion Normalized => this / Length;

        public Vector3 Axis
        {
            get
            {
                float a = 1 - (w * w);

                // Angle of 0 degrees so the axis doesn't matter
                // Return an arbitrary unit vector
                if (a <= 0.0f)
                {
                    return Vector3.Left;
                }

                a = MathF.Sqrt(a);

                return new Vector3(x / a, y / a, z / a);
            }
        }

        public Quaternion SingleCover => this * MathF.Sign(Dot(this, Identity));

        public float Angle
        {
            get
            {
                if ((1 - w * w) <= 0.0f) return 0.0f;

                if (w > MathF.Cos(1.0f / 2.0f))
                {
                    return MathF.Asin(MathF.Sqrt(x * x + y * y + z * z)) * 2.0f;
                }

                return MathF.Acos(w) * 2.0f;
            }
        }

        public bool HasNaNComponent => float.IsNaN(x) || float.IsNaN(y) || float.IsNaN(z) || float.IsNaN(w);

        public Vector3 Forward => this * Vector3.Forward;
        public Vector3 Left => this * Vector3.Left;
        public Vector3 Up => this * Vector3.Up;

        public bool Valid => Length > 0.9999f && Length < 1.00001f;

        public Quaternion(float w, float x, float y, float z)
        {
            this.w = w;
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Quaternion(Vector3 eulerAngles)
        {
            eulerAngles *= 0.5f;
            Vector3 c = new(MathF.Cos(eulerAngles.x), MathF.Cos(eulerAngles.y), MathF.Cos(eulerAngles.z));
            Vector3 s = new(MathF.Sin(eulerAngles.x), MathF.Sin(eulerAngles.y), MathF.Sin(eulerAngles.z));

            w = c.x * c.y * c.z + s.x * s.y * s.z;
            x = s.x * c.y * c.z - c.x * s.y * s.z;
            y = c.x * s.y * c.z + s.x * c.y * s.z;
            z = c.x * c.y * s.z - s.x * s.y * c.z;
        }

        public Quaternion DecomposeTwist(Vector3 axis)
        {
            Vector3 ra = new Vector3(x, y, z);
            float aDotRa = axis.Dot(ra);

            Vector3 projected = axis * aDotRa;

            Quaternion twist = new Quaternion(w, projected.x, projected.y, projected.z);

            if (aDotRa < 0f)
                twist *= -1f;

            return twist;
        }

        public static float Dot(Quaternion left, Quaternion right)
        {
            return (left.x * right.x) + (left.y * right.y) + (left.z * right.z) + (left.w * right.w);
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

        public static Quaternion FromTo(Vector3 from, Vector3 to)
        {
            Quaternion q = new();

            float dp = Vector3.Dot(from, to);

            if (dp > 0.9999f)
                return Identity;
            else if (dp < -0.9999f)
                return AngleAxis(MathF.PI, Vector3.Up);

            Vector3 a = Vector3.Cross(from, to);

            q.x = a.x;
            q.y = a.y;
            q.z = a.z;

            q.w = MathF.Sqrt((from.LengthSquared) * (to.LengthSquared)) + Vector3.Dot(from, to);
            return q.Normalized;
        }

        public static Quaternion Lerp(Quaternion a, Quaternion b, float t)
            => new Quaternion(
                MathFX.Lerp(a.x, b.x, t),
                MathFX.Lerp(a.y, b.y, t),
                MathFX.Lerp(a.z, b.z, t),
                MathFX.Lerp(a.w, b.w, t)
            );

        public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
        {
            float d = Dot(a, b);

            Quaternion aPrime = d < 0 ? a * -1f : a;
            float theta = MathF.Acos(MathF.Abs(d));

            if (d > 0.9999999f)
            {
                return Lerp(a, b, t);
            }

            return (MathF.Sin((1.0f - t) * theta) * aPrime + MathF.Sin(t * theta) * b) * (1.0f / MathF.Sin(theta));
        }

        private Mat3x3 ToMat3x3()
        {
            float sqw = w * w;
            float sqx = x * x;
            float sqy = y * y;
            float sqz = z * z;

            Mat3x3 mat = new();
            float invs = 1 / (sqx + sqy + sqz + sqw);

            mat.m00 = ( sqx - sqy - sqz + sqw) * invs; // since sqw + sqx + sqy + sqz =1/invs*invs
            mat.m11 = (-sqx + sqy - sqz + sqw) * invs;
            mat.m22 = (-sqx - sqy + sqz + sqw) * invs;
            
            float tmp1 = x * y;
            float tmp2 = z * w;
            mat.m01 = 2f * (tmp1 + tmp2) * invs;
            mat.m10 = 2f * (tmp1 - tmp2) * invs;
            
            tmp1 = x * z;
            tmp2 = y * w;
            mat.m02 = 2f * (tmp1 - tmp2) * invs;
            mat.m20 = 2f * (tmp1 + tmp2) * invs;
            tmp1 = y * z;
            tmp2 = x * w;
            mat.m12 = 2f * (tmp1 + tmp2) * invs;
            mat.m21 = 2f * (tmp1 - tmp2) * invs;     

            return mat;
        }

        public static Quaternion SafeLookAt(Vector3 dir)
        {
            return SafeLookAt(dir, Vector3.Up, Vector3.Forward);
        }

        public static Quaternion SafeLookAt(Vector3 dir, Vector3 up, Vector3 fallbackUp)
        {
            return MathF.Abs(dir.Dot(up)) > 0.999f ? LookAt(dir, fallbackUp) : LookAt(dir, up);
        }

        public static Vector3 operator*(Quaternion q, Vector3 v)
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
            =>  new Quaternion(q.w / scalar, q.x / scalar, q.y / scalar, q.z / scalar);

        public static Quaternion operator*(Quaternion q, float scalar)
            => new Quaternion(q.w * scalar, q.x * scalar, q.y * scalar, q.z * scalar);

        public static Quaternion operator*(float scalar, Quaternion q)
            => new Quaternion(q.w * scalar, q.x * scalar, q.y * scalar, q.z * scalar);

        public static Quaternion operator+(Quaternion a, Quaternion b)
            => new Quaternion(a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z);

        public override string ToString()
        {
            return $"({w:0.###}, {x:0.###}, {y:0.###}, {z:0.###})";
        }

        public static explicit operator Vector4(Quaternion q)
        {
            return new Vector4(q.x, q.y, q.z, q.w);
        }

        public static explicit operator Quaternion(Vector4 v4)
        {
            return new Quaternion(v4.w, v4.x, v4.y, v4.z);
        }

        public static explicit operator Mat3x3(Quaternion q)
        {
            return q.ToMat3x3();
        }
    }
}
