using System;
using System.Collections.Generic;
using WorldsEngine;
using WorldsEngine.Editor;

namespace Game.Interaction;

[Component]
[EditorFriendlyName("Grabbable")]
[EditorIcon(FontAwesome.FontAwesomeIcons.Hands)]
[CustomEditor(typeof(Editors.GrabbableEditor))]
class Grabbable
{
    public event Action<Entity> TriggerPressed;
    public event Action<Entity> TriggerReleased;
    public event Action<Entity> TriggerHeld;
    public event Action<Grip> OnGrabbed;
    public event Action<Grip> OnReleased;

    [EditableClass]
    public List<Grip> grips = new List<Grip>();

    public AttachedHandFlags AttachedHandFlags
    {
        get
        {
            AttachedHandFlags flags = AttachedHandFlags.None;

            foreach (Grip g in grips)
            {
                flags |= g.CurrentlyAttachedHand;
            }

            return flags;
        }
    }

    public void RunEvents(bool triggerPressed, bool triggerReleased, bool triggerHeld, Entity entity)
    {
        if (triggerPressed)
            TriggerPressed?.Invoke(entity);

        if (triggerReleased)
            TriggerReleased?.Invoke(entity);

        if (triggerHeld)
            TriggerHeld?.Invoke(entity);
    }

    internal void InvokeOnGrabbed(Grip g) => OnGrabbed?.Invoke(g);
    internal void InvokeOnReleased(Grip g) => OnReleased?.Invoke(g);
}
