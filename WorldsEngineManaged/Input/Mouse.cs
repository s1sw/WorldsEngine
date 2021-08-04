using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Input
{
    public static class Mouse
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_getMousePosition(out Vector2 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_setMousePosition(ref Vector2 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_getMouseDelta(out Vector2 pos);

        public static Vector2 Position
        {
            get
            {
                input_getMousePosition(out Vector2 pos);
                return pos;
            }

            set
            {
                input_setMousePosition(ref value);
            }
        }

        public static Vector2 PositionDelta
        {
            get
            {
                input_getMouseDelta(out Vector2 pos);
                return pos;
            }
        }
    }
}
