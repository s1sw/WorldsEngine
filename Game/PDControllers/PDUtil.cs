using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Game
{
    public static class PDUtil
    {
        public static float WrapAngle(float inputAngle)
        {
            return inputAngle % MathF.Tau;
        }

        public static float AngleToErr(float angle)
        {
            angle = WrapAngle(angle);

            if (angle > MathF.PI)
            {
                angle = MathF.Tau - angle;
                angle *= -1.0f;
            }

            return angle;
        }
    }
}
