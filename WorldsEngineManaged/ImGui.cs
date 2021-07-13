using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace WorldsEngine
{
    public class ImGui
    {
        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern bool imgui_begin(string name);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern void imgui_end();

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern void imgui_text(string text);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern bool imgui_button(string text, float sizeX, float sizeY);

        [DllImport(WorldsEngine.NATIVE_MODULE)]
        private static extern void imgui_sameLine(float offsetFromStartX, float spacing);

        public static bool Begin(string name)
        {
            return imgui_begin(name);
        }

        public static void Text(string text)
        {
            imgui_text(text);
        }

        public static bool Button(string text, float sizeX = 0.0f, float sizeY = 0.0f)
        {
            return imgui_button(text, sizeX, sizeY);
        }

        public static void SameLine(float offsetFromStartX = 0.0f, float spacing = 1.0f)
        {
            imgui_sameLine(offsetFromStartX, spacing);
        }

        public static void End()
        {
            imgui_end();
        }
    }
}
