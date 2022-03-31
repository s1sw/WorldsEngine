using WorldsEngine;
using WorldsEngine.Editor;
using WorldsEngine.Util;
using Game.World;
using ImGuiNET;

namespace Game.Editors;

public class SlidingDoorEditor : IComponentEditor
{
    public void EditEntity(Entity e)
    {
        var sd = e.GetComponent<SlidingDoor>();
        ImGui.DragFloat3("Sliding Axis", ref sd.SlideAxis);
        ImGui.DragFloat("Sliding Distance", ref sd.SlideDistance);
        ImGui.DragFloat("Speed", ref sd.Speed);
        ImGui.DragFloat3("Trigger Size", ref sd.TriggerSize);

        var t = e.Transform;
        DebugShapes.DrawBox(t.Position, sd.TriggerSize * 0.5f, t.Rotation, Colors.Green);
        DebugShapes.DrawLine(t.Position, t.Position + t.TransformDirection(sd.SlideAxis) * sd.SlideDistance, Colors.Blue);
    }
}