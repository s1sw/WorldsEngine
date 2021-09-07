using System;
using Xunit;
using WorldsEngine.Math;
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics.CodeAnalysis;

namespace WorldsEngine.Tests
{
    public class QuaternionTestData : IEnumerable<object[]>
    {
        public IEnumerator<object[]> GetEnumerator()
        {
            yield return new object[] { Quaternion.Identity };
            yield return new object[] { Quaternion.AngleAxis(MathF.PI, Vector3.Up) };
            yield return new object[] { Quaternion.AngleAxis(MathF.PI, Vector3.Left) };
            yield return new object[] { Quaternion.AngleAxis(MathF.Tau, Vector3.Left) };
            yield return new object[] { Quaternion.AngleAxis(3.62434f, Vector3.Right) };
        }

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }

    class ApproximateQuaternionComparer : IEqualityComparer<Quaternion>
    {
        private bool FloatApproximatelyEqual(float a, float b)
        {
            return MathF.Abs(a - b) < 0.001f;
        }

        public bool Equals(Quaternion a, Quaternion b)
        {
            return
                FloatApproximatelyEqual(a.w, b.w) &&
                FloatApproximatelyEqual(a.x, b.x) &&
                FloatApproximatelyEqual(a.y, b.y) &&
                FloatApproximatelyEqual(a.z, b.z);
        }

        public int GetHashCode([DisallowNull] Quaternion obj)
        {
            return obj.GetHashCode();
        }
    }

    public class Mat3x3Tests
    {
        [Theory]
        [ClassData(typeof(QuaternionTestData))]
        public void QuaternionRoundTrip(Quaternion quat)
        {
            Mat3x3 matrix = quat.SingleCover.ToMat3x3();

            Assert.Equal(((Quaternion)matrix).SingleCover, quat.SingleCover, new ApproximateQuaternionComparer());
        }

        [Fact]
        public void QuaternionToMatrix()
        {
            Assert.Equal(new Quaternion(0.0f, 1.0f, 0.0f, 0.0f).ToMat3x3(), new Mat3x3(
                new Vector3(1.0f, 0.0f, 0.0f),
                new Vector3(0.0f, -1.0f, 0.0f),
                new Vector3(0.0f, 0.0f, - 1.0f)
            ));

            Assert.Equal(Quaternion.AngleAxis(5.0f, Vector3.Forward).ToMat3x3(), new Mat3x3(
                new Vector3(0.2836622f, -0.9589243f, 0.0f),
                new Vector3(0.9589243f, 0.2836622f, 0.0f),
                new Vector3(0.0f, 0.0f, 1.0f)
            )); 
        }

        [Fact]
        public void MatrixToQuaternion()
        {
            Assert.Equal((Quaternion)new Mat3x3(
                new Vector3(1.0f, 0.0f, 0.0f),
                new Vector3(0.0f, -1.0f, 0.0f),
                new Vector3(0.0f, 0.0f, -1.0f)
            ), new Quaternion(0.0f, 1.0f, 0.0f, 0.0f));

            Assert.Equal(((Quaternion)new Mat3x3(
                new Vector3(0.2836622f, -0.9589243f, 0.0f),
                new Vector3(0.9589243f, 0.2836622f, 0.0f),
                new Vector3(0.0f, 0.0f, 1.0f)
            )).SingleCover, (new Quaternion(-0.8011436f, 0.0f, 0.0f, 0.5984721f)).SingleCover, new ApproximateQuaternionComparer());
        }
    }
}
