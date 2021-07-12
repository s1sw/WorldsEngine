using System.Runtime.InteropServices;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Sequential)]
    struct Transform
    {
        public Vector3 position;
        public Quaternion rotation;
        public Vector3 scale;
    }
}
