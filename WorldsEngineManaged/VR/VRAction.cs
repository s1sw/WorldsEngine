using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public class VRAction
    {
        [DllImport(WorldsEngine.NativeModule, CharSet = CharSet.Ansi)]
        private static extern ulong vr_getActionHandle(string actionPath);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_getActionHeld(ulong handle);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_getActionPressed(ulong handle);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_getActionReleased(ulong handle);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void vr_getActionVector2(ulong handle, out Vector2 vec);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void vr_triggerHaptics(ulong handle, float timeFromNow, float duration, float frequency, float amplitude);

        private readonly ulong _actionHandle;

        public VRAction(string actionPath)
        {
            if (!VR.Enabled)
                throw new InvalidOperationException("Can't create a VRAction when VR isn't enabled");
            _actionHandle = vr_getActionHandle(actionPath);
        }

        public bool Held => vr_getActionHeld(_actionHandle);
        public bool Pressed => vr_getActionPressed(_actionHandle);
        public bool Released => vr_getActionReleased(_actionHandle);
        public Vector2 Vector2Value
        {
            get
            {
                Vector2 v = new Vector2();

                vr_getActionVector2(_actionHandle, out v);

                return v;
            }
        }

        public void TriggerHaptics(float timeFromNow, float duration, float frequency, float amplitude)
        {
            vr_triggerHaptics(_actionHandle, timeFromNow, duration, frequency, amplitude);
        }
    }
}
