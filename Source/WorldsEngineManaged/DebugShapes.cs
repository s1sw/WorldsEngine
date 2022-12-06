using WorldsEngine.Math;
using System.Runtime.InteropServices;

namespace WorldsEngine;

public static class DebugShapes
{
    [DllImport(Engine.NativeModule)]
    private static extern void debugshapes_drawLine(Vector3 p0, Vector3 p1, Vector4 color);
    
    [DllImport(Engine.NativeModule)]
    private static extern void debugshapes_drawCircle(Vector3 center, float radius, Quaternion rotation, Vector4 color);
    
    [DllImport(Engine.NativeModule)]
    private static extern void debugshapes_drawSphere(Vector3 center, Quaternion rotation, float radius, Vector4 color);
    
    [DllImport(Engine.NativeModule)]
    private static extern void debugshapes_drawBox(Vector3 center, Quaternion rotation, Vector3 halfExtents);

    public static void DrawLine(Vector3 p0, Vector3 p1, Vector4 color)
        => debugshapes_drawLine(p0, p1, color);
    
    public static void DrawCircle(Vector3 center, float radius, Quaternion rotation, Vector4 color)
        => debugshapes_drawCircle(center, radius, rotation, color);
    
    public static void DrawSphere(Vector3 center, float radius, Quaternion rotation, Vector4 color)
        => debugshapes_drawSphere(center, rotation, radius, color);
    
    public static void DrawBox(Vector3 center, Vector3 halfExtents, Quaternion rotation, Vector4 color)
        => debugshapes_drawBox(center, rotation, halfExtents);
}