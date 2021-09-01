using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Input
{
    public enum MouseButton
    {
        Left = 1,
        Middle = 2,
        Right = 3
    }

    public static class Mouse
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_getMousePosition(out Vector2 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_setMousePosition(ref Vector2 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void input_getMouseDelta(out Vector2 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getMouseButtonPressed(MouseButton button);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getMouseButtonReleased(MouseButton button);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getMouseButtonHeld(MouseButton button);

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

        public static bool ButtonPressed(MouseButton button) => input_getMouseButtonPressed(button);

        public static bool ButtonHeld(MouseButton button) => input_getMouseButtonHeld(button);

        public static bool ButtonReleased(MouseButton button) => input_getMouseButtonReleased(button);
    }
}
