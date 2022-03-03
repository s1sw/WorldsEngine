using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace WorldsEngine.Input;

public class Action
{
    public string ID = "Action";
}

public class ButtonEventArgs : EventArgs
{
    /// <summary>
    /// If true, the button was pressed.
    /// If false, the button was released.
    /// </summary>
    public bool Pressed;
}

public class ButtonAction : Action
{
    public event EventHandler<ButtonEventArgs>? OnChange;
    public bool CurrentlyPressed;
}

public class AxisAction : Action
{
    public float CurrentValue;
}

internal enum NativeEventKind : int
{
    Invalid = -1,
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    ControllerButtonDown,
    ControllerButtonUp,
    ControllerAxisMotion
}

[StructLayout(LayoutKind.Explicit)]
internal struct NativeEvent 
{
    [FieldOffset(0)]
    public NativeEventKind EventKind;
    
    [FieldOffset(4)]
    public KeyCode KeyCode;

    [FieldOffset(4)]
    public int MouseButtonIndex;

    [FieldOffset(4)]
    public ControllerButton ControllerButton;

    [FieldOffset(4)]
    public ControllerAxis ControllerAxis;

    [FieldOffset(8)]
    public float AxisValue;
}

public static class InputSystem
{
    internal static void EndFrame()
    {
        Controller.NextFrame();
    }

    internal static unsafe void ProcessNativeEvent(NativeEvent* evt)
    {
        NativeEvent e = *evt;

        switch (e.EventKind)
        {
        case NativeEventKind.ControllerButtonDown:
        case NativeEventKind.ControllerButtonUp:
        case NativeEventKind.ControllerAxisMotion:
            Controller.ProcessNativeEvent(e);
            break;
        }
    }
}
