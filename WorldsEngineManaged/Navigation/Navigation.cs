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

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool navigation_getClosestPointOnMesh(Vector3 point, ref Vector3 outPoint, Vector3 searchExtent);

        public static NavigationPath FindPath(Vector3 startPos, Vector3 endPos) => new(navigation_findPath(startPos, endPos));

        public static bool GetClosestPoint(Vector3 point, out Vector3 foundPoint)
        {
            foundPoint = Vector3.Zero;
            return navigation_getClosestPointOnMesh(point, ref foundPoint, new Vector3(1.0f, 3.5f, 1.0f));
        }

        public static bool GetClosestPoint(Vector3 point, out Vector3 foundPoint, Vector3 searchExtent)
        {
            foundPoint = Vector3.Zero;
            return navigation_getClosestPointOnMesh(point, ref foundPoint, searchExtent);
        }
    }
}
