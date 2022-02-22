using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Navigation
{
    public class NavigationPath
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void navigation_deletePath(IntPtr path);

        [StructLayout(LayoutKind.Sequential)]
        struct NativeNavigationPath
        {
            [MarshalAs(UnmanagedType.I1)]
            public bool Valid;
            public int NumPoints;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public Vector3[] PathPoints;
        }

        private readonly NativeNavigationPath _nativePath;

        internal NavigationPath(IntPtr nativePtr)
        {
            _nativePath = Marshal.PtrToStructure<NativeNavigationPath>(nativePtr);
            if (!_nativePath.Valid) _nativePath.NumPoints = 0;
            navigation_deletePath(nativePtr);
        }

        public int NumPoints
        {
            get => _nativePath.NumPoints;
        }

        public bool Valid => _nativePath.Valid;
        public Vector3 this[int index]
        {
            get
            {
                if (index >= _nativePath.NumPoints || index < 0) throw new ArgumentOutOfRangeException(nameof(index));
                return _nativePath.PathPoints[index];
            }
        }
    }
    public static class NavigationSystem
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern IntPtr navigation_findPath(Vector3 startPos, Vector3 endPos);

        public static NavigationPath FindPath(Vector3 startPos, Vector3 endPos) => new(navigation_findPath(startPos, endPos));
    }
}
