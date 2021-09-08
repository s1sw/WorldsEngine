using System;
using System.Collections.Generic;
using WorldsEngine;

namespace Game.Interaction
{
    [Component]
    [EditorFriendlyName("C# Grabbable")]
    class Grabbable
    {
        public event Action<Entity> TriggerPressed;
        public event Action<Entity> TriggerReleased;
        public event Action<Entity> TriggerHeld;

        [EditableClass]
        public List<Grip> grips = new List<Grip>();

        public void RunEvents(bool triggerPressed, bool triggerReleased, bool triggerHeld, Entity entity)
        {
            if (triggerPressed)
            {
                TriggerPressed?.Invoke(entity);
                Logger.Log("Calling TriggerPressed");
            }

            if (triggerReleased)
                TriggerReleased?.Invoke(entity);

            if (triggerHeld)
                TriggerHeld?.Invoke(entity);
        }
    }
}
