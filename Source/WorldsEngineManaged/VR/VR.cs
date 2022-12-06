using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public enum VRHand : int
    {
        Left,
        Right
    }

    public static class VR
    {
        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_enabled();

        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_hasInputFocus();

        [DllImport(Engine.NativeModule)]
        private static extern void vr_getHeadTransform(ref Transform transform);

		private static bool _enabled = false;

		static VR()
		{
			_enabled = vr_enabled();
		}

        public static Transform HMDTransform
        {
            get
            {
				if (!Enabled) throw new InvalidOperationException();
                Transform t = new Transform();
                vr_getHeadTransform(ref t);
                return t;
            }
        }

        public static bool Enabled => _enabled;

        public static bool HasInputFocus => vr_hasInputFocus();
    }
}
