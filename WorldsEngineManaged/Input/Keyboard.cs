using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Input
{
    public static class Keyboard
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getKeyHeld(KeyCode code);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getKeyPressed(KeyCode code);
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool input_getKeyReleased(KeyCode code);

        public static bool KeyHeld(KeyCode code)
        {
            return input_getKeyHeld(code);
        }

        public static bool KeyPressed(KeyCode code)
        {
            return input_getKeyPressed(code);
        }

        public static bool KeyReleased(KeyCode code)
        {
            return input_getKeyReleased(code);
        }
    }
}
