using System;

namespace WorldsEngine.Input;

public enum ControllerButton : int
{
    Invalid = -1,
    A,
    B,
    X,
    Y,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Misc,
    Paddle1,
    Paddle2,
    Paddle3,
    Paddle4,
    Touchpad
}

public enum ControllerAxis : int
{
    Invalid = -1,
    LeftX,
    LeftY,
    RightX,
    RightY,
    TriggerLeft,
    TriggerRight
}

public static class Controller
{
    public static bool ButtonHeld(ControllerButton button) => _heldButtons[(int)button];
    public static bool ButtonPressed(ControllerButton button)
        => _heldButtons[(int)button] && !_lastHeldButtons[(int)button];
    public static bool ButtonReleased(ControllerButton button)
        => !_heldButtons[(int)button] && _lastHeldButtons[(int)button];
    public static float AxisValue(ControllerAxis axis) => _axisValues[(int)axis];
    public static float DeadzonedAxisValue(ControllerAxis axis)
    {
        float v = AxisValue(axis);
        if (MathF.Abs(v) < 0.1f)
            v = 0.0f;

        return v;
    }

    private static bool[] _heldButtons = new bool[21];
    private static bool[] _lastHeldButtons = new bool[21];
    private static float[] _axisValues = new float[6];

    internal static void NextFrame()
    {
        for (int i = 0; i < 21; i++)
        {
            _lastHeldButtons[i] = _heldButtons[i];
        }
    }

    internal static void ProcessNativeEvent(NativeEvent e)
    {
        switch (e.EventKind)
        {
        case NativeEventKind.ControllerButtonDown:
            _heldButtons[(int)e.ControllerButton] = true;
            break;
        case NativeEventKind.ControllerButtonUp:
            _heldButtons[(int)e.ControllerButton] = false;
            break;
        case NativeEventKind.ControllerAxisMotion:
            _axisValues[(int)e.ControllerAxis] = e.AxisValue;
            break;
        }
    }
}
