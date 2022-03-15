using WorldsEngine;
using WorldsEngine.Editor;
using WorldsEngine.Math;
using Game.Interaction;
using ImGuiNET;

namespace Game.Editors;

public class GrabbableEditor : IComponentEditor
{
    private TransformEditContext _transformEditContext = new();
    private Entity _handEntity = Entity.Null;
    
    private void EditManualGrip(Entity e, Grip g)
    {
        if (!_transformEditContext.CurrentlyUsing)
        {
            if (ImGui.Button("Change Offset"))
            {
                Transform t = new(g.position, g.rotation);
                _transformEditContext.StartUsing(t.TransformBy(e.Transform));
                _handEntity = Registry.Create();
                var wo = Registry.AddComponent<WorldObject>(_handEntity);
                wo.Mesh = AssetDB.PathToId("Models/VRHands/hand_placeholder_r.wmdl");
            }
        }
        else
        {
            _transformEditContext.Update();
            _handEntity.Transform = _transformEditContext.Transform;
            Transform t = _transformEditContext.Transform.TransformByInverse(e.Transform);
            g.position = t.Position;
            g.rotation = t.Rotation;

            if (ImGui.Button("Done"))
            {
                _transformEditContext.StopUsing();
                Registry.Destroy(_handEntity);
                _handEntity = Entity.Null;
            }
        }
    }
    
    private void EditBoxGrip(Entity e, Grip g)
    {
        ImGui.DragFloat3("Box Extents", ref g.BoxExtents);
    }
    
    private void EditSphereGrip(Entity e, Grip g)
    {
        ImGui.DragFloat("Sphere Radius", ref g.SphereRadius);
    }

    public void EditEntity(Entity e)
    {
        Grabbable grabbable = e.GetComponent<Grabbable>();
        
        if (ImGui.TreeNode("Grips"))
        {
            ImGui.Text($"{grabbable.grips.Count} grips");
            
            int i = 0;
            foreach (Grip g in grabbable.grips)
            {
                if (ImGui.TreeNode($"Grip {i}"))
                {
                    EditorUtils.EnumDropdown("Type", ref g.Type);
                    
                    switch (g.Type)
                    {
                    case GripType.Manual:
                        EditManualGrip(e, g);
                        break;
                    case GripType.Box:
                        EditBoxGrip(e, g);
                        break;
                    case GripType.Sphere:
                        EditSphereGrip(e, g);
                        break;
                    }
                    
                    ImGui.TreePop();
}

                i++;
            }
            
            ImGui.TreePop();
        }
    }
}