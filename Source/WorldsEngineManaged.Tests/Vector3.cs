using System;
using Xunit;
using WorldsEngine.Math;

namespace WorldsEngine.Tests
{
    public class Vector3Tests
    {
        [Fact]
        public void TestAdd()
        {
            Vector3 v = new(-2.0f, 1.0f, 2.0f);
            Vector3 v2 = new(3.0f, 2.0f, 1.0f);

            Assert.Equal(new Vector3(1.0f, 3.0f, 3.0f), v + v2);
        }

        [Fact]
        public void TestMultiply()
        {
            Vector3 v = new(5.0f, 10.0f, 20.0f);
            Vector3 v2 = new(12.0f, 6.0f, 3.0f);

            Assert.Equal(new Vector3(60.0f), v * v2);
        }

        [Fact]
        public void TestDot()
        {
            Vector3 v = new(0.0f, 5.0f, 0.0f);
            Vector3 v2 = new(0.0f, -5.0f, 0.0f);

            Assert.Equal(25.0f, v.Dot(v));
            Assert.Equal(-25.0f, v.Dot(v2));
        }

        [Fact]
        public void TestCross()
        {
            Assert.Equal(new Vector3(0.0f, 0.0f, 1.0f), new Vector3(1.0f, 0.0f, 0.0f).Cross(new Vector3(0.0f, 1.0f, 0.0f)));
        }
    }
}
