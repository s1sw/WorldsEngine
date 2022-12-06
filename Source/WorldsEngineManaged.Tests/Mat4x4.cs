using Xunit;
using WorldsEngine;
using WorldsEngine.Math;
using System;

namespace WorldsEngine.Tests;

public class Mat4x4Tests
{
    [Fact]
    public void IdentityTransformConversion()
    {
        Mat4x4 mat = new(new Vector4(1.0f, 0.0f, 0.0f, 0.0f), new Vector4(0.0f, 1.0f, 0.0f, 0.0f), new Vector4(0.0f, 0.0f, 1.0f, 0.0f), new Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        Transform t = mat.ToTransform;
        Assert.Equal(t.Position, Vector3.Zero);
        Assert.Equal(t.Rotation, Quaternion.Identity);
        Assert.Equal(t.Scale, Vector3.One);
    }

    [Fact]
    public void NonUniformScaleTransformConversion()
    {
        Mat4x4 mat = new(new Vector4(5.0f, 0.0f, 0.0f, 0.0f), new Vector4(0.0f, 2.0f, 0.0f, 0.0f), new Vector4(0.0f, 0.0f, 3.0f, 0.0f), new Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        Transform t = mat.ToTransform;
        Assert.Equal(Vector3.Zero, t.Position);
        Assert.Equal(Quaternion.Identity, t.Rotation);
        Assert.Equal(new Vector3(5.0f, 2.0f, 3.0f), t.Scale);
    }

    [Fact]
    public void RotatedTransformConversion()
    {
        Mat4x4 mat = new(
            new Vector4(1f, 0f, 0f, 0f),
            new Vector4(0f, 0f, 1f, 0f),
            new Vector4(0f, -1f, 0f, 0f),
            new Vector4(0f, 0f, 0f, 1f)
        );

        Transform t = mat.ToTransform;
        Assert.Equal(Quaternion.AngleAxis(MathF.PI * 0.5f, new Vector3(1f, 0f, 0f)), t.Rotation, new ApproximateQuaternionComparer());
        Assert.Equal(Vector3.Zero, t.Position);
        Assert.Equal(Vector3.One, t.Scale);
    }
}