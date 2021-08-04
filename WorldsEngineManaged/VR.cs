using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public static class VR
    {
        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool vr_enabled();

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void vr_getHeadTransform(float predictionTime, ref Transform transform);
        
        public static Transform HMDTransform
        {
            get
            {
                Transform t = new Transform();
                vr_getHeadTransform(0.0f, ref t);
                return t;
            }
        }

        public static bool Enabled => vr_enabled();
    }
}
