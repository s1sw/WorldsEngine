using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game
{
    internal static class VRTransforms
    {
        public static float Scale = 1.0f;
        public static Transform HMDTransform
        {
            get
            {
                Transform t = VR.HMDTransform;
                t.Position *= Scale;
                return t;
            }
        }

        public static Transform LeftHandTransform
        {
            get
            {
                Transform t = VR.LeftHandTransform;
                t.Position *= Scale;
                return t;
            }
        }

        public static Transform RightHandTransform
        {
            get
            {
                Transform t = VR.RightHandTransform;
                t.Position *= Scale;
                return t;
            }
        }
    }
}
