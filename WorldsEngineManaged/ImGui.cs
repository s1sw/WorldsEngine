using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Xml.Linq;
using WorldsEngine.Math;

namespace WorldsEngine
{
    [Obsolete("Use ImGuiNET instead.")]
    public class ImGui
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_begin(IntPtr name);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_end();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_text(IntPtr text);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_dragInt(IntPtr text, ref int value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_dragFloat(IntPtr text, ref float value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_dragFloat3(IntPtr text, ref Vector3 value);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_button(IntPtr text, float sizeX, float sizeY);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_sameLine(float offsetFromStartX, float spacing);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_openPopup(IntPtr name);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_beginPopup(IntPtr name);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_endPopup();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void imgui_closeCurrentPopup();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool imgui_collapsingHeader(IntPtr text);

        public static bool Begin(string name)
        {
            IntPtr interopName = Marshal.StringToCoTaskMemUTF8(name);
            bool result = imgui_begin(interopName);
            Marshal.FreeCoTaskMem(interopName);

            return result;
        }

        public static bool BeginPopup(string name)
        {
            IntPtr interopName = Marshal.StringToCoTaskMemUTF8(name);

            bool result = imgui_beginPopup(interopName);

            Marshal.FreeCoTaskMem(interopName);

            return result;
        }

        public static void EndPopup()
        {
            imgui_endPopup();
        }

        public static void OpenPopup(string name)
        {
            IntPtr interopName = Marshal.StringToCoTaskMemUTF8(name);

            imgui_openPopup(interopName);

            Marshal.FreeCoTaskMem(interopName);
        }

        public static void CloseCurrentPopup()
        {
            imgui_closeCurrentPopup();
        }

        public static void Text(string text)
        {
            IntPtr interopText = Marshal.StringToCoTaskMemUTF8(text);

            imgui_text(interopText);

            Marshal.FreeCoTaskMem(interopText);
        }

        public static bool DragInt(string label, ref int value)
        {
            IntPtr interopLabel = Marshal.StringToCoTaskMemUTF8(label);
            
            bool result = imgui_dragInt(interopLabel, ref value);

            Marshal.FreeCoTaskMem(interopLabel);

            return result;
        }

        public static bool DragFloat(string label, ref float value)
        {
            IntPtr interopLabel = Marshal.StringToCoTaskMemUTF8(label);

            bool result = imgui_dragFloat(interopLabel, ref value);

            Marshal.FreeCoTaskMem(interopLabel);

            return result;
        }

        public static bool DragFloat3(string label, ref Vector3 value)
        {
            IntPtr interopLabel = Marshal.StringToCoTaskMemUTF8(label);

            bool result = imgui_dragFloat3(interopLabel, ref value);

            Marshal.FreeCoTaskMem(interopLabel);

            return result;
        }

        public static bool Button(string text, float sizeX = 0.0f, float sizeY = 0.0f)
        {
            IntPtr interopText = Marshal.StringToCoTaskMemUTF8(text);

            bool result = imgui_button(interopText, sizeX, sizeY);

            Marshal.FreeCoTaskMem(interopText);

            return result;
        }

        public static void SameLine(float offsetFromStartX = 0.0f, float spacing = 1.0f)
        {
            imgui_sameLine(offsetFromStartX, spacing);
        }

        public static void End()
        {
            imgui_end();
        }
        
        public static bool CollapsingHeader(string header)
{
            IntPtr interopText = Marshal.StringToCoTaskMemUTF8(header);

            bool result = imgui_collapsingHeader(interopText);

            Marshal.FreeCoTaskMem(interopText);

            return result;
        }
    }
}
