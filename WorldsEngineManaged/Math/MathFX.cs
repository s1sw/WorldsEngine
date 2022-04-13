using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Math
{
    public static class MathFX
    {
        public const float DegreesToRadians = MathF.PI / 180.0f;
        public const float RadiansToDegrees = 180.0f / MathF.PI;

        public static float Clamp(float value, float minimum, float maximum)
        {
            return MathF.Max(MathF.Min(value, maximum), minimum);
        }

        /// <summary>
        /// Clamps a value between 0 and 1.
        /// </summary>
        public static float Saturate(float val)
        {
            return Clamp(val, 0.0f, 1.0f);
        }

        public static Vector3 Clamp(Vector3 value, Vector3 minimum, Vector3 maximum)
        {
            return new Vector3(
                Clamp(value.x, minimum.x, maximum.x),
                Clamp(value.y, minimum.y, maximum.y),
                Clamp(value.z, minimum.z, maximum.z)
            );
        }

        public static float Lerp(float a, float b, float t) => a + (b - a) * t;
    }
}
