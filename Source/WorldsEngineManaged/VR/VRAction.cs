using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Sequential)]
    struct BooleanActionState
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool CurrentState;
        [MarshalAs(UnmanagedType.I1)]
        public bool ChangedSinceLastFrame;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct FloatActionState
    {
        public float CurrentState;
        [MarshalAs(UnmanagedType.I1)]
        public bool ChangedSinceLastFrame;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct Vector2ActionState
    {
        public Vector2 CurrentState;
        [MarshalAs(UnmanagedType.I1)]
        public bool ChangedSinceLastFrame;
    }

    public static class VRSubactions
    {
        public static string LeftHand => "/user/hand/left";
        public static string RightHand => "/user/hand/right";
    }

    public class VRAction
    {
        [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
        private static extern ulong vr_getActionHandle(string actionSet, string actionPath);
        [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
        private static extern ulong vr_getSubactionHandle(string subaction);

        [DllImport(Engine.NativeModule)]
        private static extern BooleanActionState vr_getBooleanActionState(ulong actionHandle, ulong subactionHandle);

        [DllImport(Engine.NativeModule)]
        private static extern FloatActionState vr_getFloatActionState(ulong actionHandle, ulong subactionHandle);

        [DllImport(Engine.NativeModule)]
        private static extern Vector2ActionState vr_getVector2fActionState(ulong actionHandle, ulong subactionHandle);

        [DllImport(Engine.NativeModule)]
        private static extern void vr_getPoseActionState(ulong actionHandle, ulong subactionHandle, ref Transform t);

        [DllImport(Engine.NativeModule)]
        private static extern void vr_triggerHaptics(float duration, float frequency, float amplitude,
            ulong actionHandle, ulong subactionHandle);

        private readonly ulong _actionHandle;
        private readonly ulong _subactionHandle = 0;

        public VRAction(string actionSet, string actionPath, string? subaction = null)
        {
            if (!VR.Enabled)
                throw new InvalidOperationException("Can't create a VRAction when VR isn't enabled");
            
            _actionHandle = vr_getActionHandle(actionSet, actionPath);
            
            if (subaction != null)
            {
                _subactionHandle = vr_getSubactionHandle(subaction);
            }
        }

        public bool Held
        {
            get
            {
                var actionState = vr_getBooleanActionState(_actionHandle, _subactionHandle);
                return actionState.CurrentState;
            }
        }

        public bool Pressed
        {
            get
            {
                var actionState = vr_getBooleanActionState(_actionHandle, _subactionHandle);
                return actionState.CurrentState && actionState.ChangedSinceLastFrame;
            }
        }

        public bool Released
        {
            get
            {
                var actionState = vr_getBooleanActionState(_actionHandle, _subactionHandle);
                return !actionState.CurrentState && actionState.ChangedSinceLastFrame;
            }
        }

        public float FloatValue => vr_getFloatActionState(_actionHandle, _subactionHandle).CurrentState;

        public Vector2 Vector2Value => vr_getVector2fActionState(_actionHandle, _subactionHandle).CurrentState;

        public Transform Pose
        {
            get
            {
                Transform t = new();
                vr_getPoseActionState(_actionHandle, _subactionHandle, ref t);
                return t;
            }
        }

        public void TriggerHaptics(float duration, float frequency, float amplitude)
        {
            vr_triggerHaptics(duration, frequency, amplitude, _actionHandle, _subactionHandle);
        }
    }
}
