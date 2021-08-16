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
        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_enabled();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void vr_getHeadTransform(float predictionTime, ref Transform transform);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void vr_getHandTransform(VRHand hand, ref Transform transform);
		
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
                vr_getHeadTransform(0.0f, ref t);
                ConvertCoordinateSystem(ref t);
                return t;
            }
        }

        public static bool Enabled => _enabled;

        public static Transform LeftHandTransform
        {
            get
            {
				if (!Enabled) throw new InvalidOperationException();
                Transform t = new Transform();
                vr_getHandTransform(VRHand.Left, ref t);
                ConvertCoordinateSystem(ref t);
                return t;
            }
        }

        public static Transform RightHandTransform
        {
            get
            {
				if (!Enabled) throw new InvalidOperationException();
                Transform t = new Transform();
                vr_getHandTransform(VRHand.Right, ref t);
                ConvertCoordinateSystem(ref t);
                return t;
            }
        }
        
        private static void ConvertCoordinateSystem(ref Transform t)
        {
            Vector3 flipVec = new Vector3(-1.0f, 1.0f, -1.0f);

            t.Position *= flipVec;

            t.Rotation = Quaternion.AngleAxis(t.Rotation.Angle, t.Rotation.Axis * flipVec);
        }
    }
}
