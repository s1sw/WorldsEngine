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

        public static Vector3 Clamp(Vector3 value, Vector3 minimum, Vector3 maximum)
        {
            return new Vector3(
                Clamp(value.x, minimum.x, maximum.x),
                Clamp(value.y, minimum.y, maximum.y),
                Clamp(value.z, minimum.z, maximum.z)
            );
        }
    }
}
