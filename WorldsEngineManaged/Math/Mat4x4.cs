using System;
using System.Runtime.InteropServices;
namespace WorldsEngine.Math;

[StructLayout(LayoutKind.Sequential)]
public struct Mat4x4
{
    public float m00, m10, m20, m30;
    public float m01, m11, m21, m31;
    public float m02, m12, m22, m32;
    public float m03, m13, m23, m33;

    public Vector4 this[int index] => index switch
    {
        0 => new Vector4(m00, m10, m20, m30),
        1 => new Vector4(m01, m11, m21, m31),
        2 => new Vector4(m02, m12, m22, m32),
        3 => new Vector4(m03, m13, m23, m33),
        _ => throw new IndexOutOfRangeException()
    };

    public Mat4x4 Transpose => new(
        new Vector4(m00, m10, m20, m30),
        new Vector4(m01, m11, m21, m31),
        new Vector4(m02, m12, m22, m32),
        new Vector4(m03, m13, m23, m33)
    );

    public Transform ToTransform
    {
        get
        {
            Transform t = new();

            t.Scale.x = new Vector3(m00, m10, m20).Length;
            t.Scale.y = new Vector3(m01, m11, m21).Length;
            t.Scale.z = new Vector3(m02, m12, m22).Length;
            t.Position = this[3].TruncateToVec3();

            // oh dear god it's quaternion time!
            Mat3x3 as3x3 = new(
                new Vector3(m00, m01, m02),
                new Vector3(m10, m11, m12),
                new Vector3(m20, m21, m22)
            );
            t.Rotation = ((Quaternion)as3x3.Transpose).Normalized;

            return t;
        }
    }

    public Mat4x4(Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3)
    {
        m00 = c0.x;
        m01 = c0.y;
        m02 = c0.z;
        m03 = c0.w;

        m10 = c1.x;
        m11 = c1.y;
        m12 = c1.z;
        m13 = c1.w;

        m20 = c2.x;
        m21 = c2.y;
        m22 = c2.z;
        m23 = c2.w;

        m30 = c3.x;
        m31 = c3.y;
        m32 = c3.z;
        m33 = c3.w;
    }

}