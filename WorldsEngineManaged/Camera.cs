using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public class Camera
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void camera_getPosition(IntPtr camPtr, out Vector3 pos);
        [DllImport(WorldsEngine.NativeModule)]
        private unsafe static extern void camera_setPosition(IntPtr camPtr, Vector3* pos);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void camera_getRotation(IntPtr camPtr, out Quaternion rotation);
        [DllImport(WorldsEngine.NativeModule)]
        private unsafe static extern void camera_setRotation(IntPtr camPtr, Quaternion* rotation);

        public static Camera Main { get; internal set; }

        public Vector3 Position
        {
            get
            {
                camera_getPosition(ptr, out Vector3 pos);
                return pos;
            }

            set
            {
                unsafe
                {
                    camera_setPosition(ptr, &value);
                }
            }
        }

        public Quaternion Rotation
        {
            get
            {
                camera_getRotation(ptr, out Quaternion rotation);
                return rotation;
            }

            set
            {
                unsafe
                {
                    camera_setRotation(ptr, &value);
                }
            }
        }

        private readonly IntPtr ptr;

        internal Camera(IntPtr cameraPtr)
        {
            ptr = cameraPtr;
        }

        public Vector3 TransformPoint(Vector3 point)
        {
            return Position + (Rotation * point);
        }
    }
}
