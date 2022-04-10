using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    public class Time
    {
        public static float DeltaTime { get; internal set; }

        public static double CurrentTime { get; internal set; }
        public static float InterpolationAlpha { get; internal set; }
    }
}
