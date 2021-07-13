using System.Runtime.InteropServices;

namespace WorldsEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float w, x, y, z;

        public static Quaternion Identity => new Quaternion(1.0f, 0.0f, 0.0f, 0.0f);

        public Quaternion(float w, float x, float y, float z)
        {
            this.w = w;
            this.x = x;
            this.y = y;
            this.z = z;
        }
    }
}
