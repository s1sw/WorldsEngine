using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace WorldsEngine
{
    public class ImGuiWindow : IDisposable
    {
        internal ImGuiWindow()
        {
        }

        public void Dispose()
        {
            ImGui.End();
        }
    }

    public class ImGui
    {
        private delegate bool BeginDelegate(string name);
        private delegate void TextDelegate(string text);
        private delegate bool ButtonDelegate(string text, float sizeX, float sizeY);
        private delegate void SameLineDelegate(float offsetFromStartX, float spacing);
        private delegate void EndDelegate();

        private static BeginDelegate begin;
        private static TextDelegate text;
        private static ButtonDelegate button;
        private static SameLineDelegate sameLine;
        private static EndDelegate end;

        private static readonly ImGuiWindow window = new ImGuiWindow();

        [StructLayout(LayoutKind.Sequential)]
        internal struct ImGuiFuncPtrs
        {
            public IntPtr Begin;
            public IntPtr Text;
            public IntPtr Button;
            public IntPtr SameLine;
            public IntPtr End;
        }

        internal static void SetFunctionPointers(ImGuiFuncPtrs funcPtrs)
        {
            // TODO: There must be an easier way to do this.
            begin = Marshal.GetDelegateForFunctionPointer<BeginDelegate>(funcPtrs.Begin);
            text = Marshal.GetDelegateForFunctionPointer<TextDelegate>(funcPtrs.Text);
            button = Marshal.GetDelegateForFunctionPointer<ButtonDelegate>(funcPtrs.Button);
            sameLine = Marshal.GetDelegateForFunctionPointer<SameLineDelegate>(funcPtrs.SameLine);
            end = Marshal.GetDelegateForFunctionPointer<EndDelegate>(funcPtrs.End);
        }

        public static ImGuiWindow Window(string title)
        {
            Begin(title);
            return window;
        }

        internal static bool Begin(string name)
        {
            return begin(name);
        }

        public static void Text(string text)
        {
            ImGui.text(text);
        }

        public static bool Button(string text, float sizeX = 0.0f, float sizeY = 0.0f)
        {
            return button(text, sizeX, sizeY);
        }

        public static void SameLine(float offsetFromStartX = 0.0f, float spacing = 1.0f)
        {
            sameLine(offsetFromStartX, spacing);
        }

        internal static void End()
        {
            end();
        }
    }
}