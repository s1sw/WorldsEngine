using System;
namespace WorldsEngine.Editor;

[AttributeUsage(AttributeTargets.Class)]
public class GizmoAttribute : Attribute
{
    public readonly string GizmoPath;

    public GizmoAttribute(string gizmoPath)
    {
        GizmoPath = gizmoPath;
    }
}