using System;
using WorldsEngine.ECS;

namespace WorldsEngine.Editor;

public class TransformEditContext
{
    public bool CurrentlyUsing = false;
    public string? FieldName;
    public Entity? Entity;
    
    public Transform Transform
    {
        get
        {
            if (Entity == null)
                throw new InvalidOperationException("TransformEditContext must be started first");

            return Entity.Value.Transform;
        }
    }

    public void StartUsing(Transform t)
    {
        CurrentlyUsing = true;
        Entity = Registry.Create();
        Registry.SetTransform(Entity.Value, t);
    }
    
    public void Update()
    {
        if (Entity == null)
            throw new InvalidOperationException("StartUsing() must be called before calling Update()");

        Editor.OverrideHandle(Entity.Value);
    }

    public void StopUsing()
    {
        if (Entity == null) return;
        Registry.Destroy(Entity.Value);
        Entity = null;
        CurrentlyUsing = false;
    }
}
