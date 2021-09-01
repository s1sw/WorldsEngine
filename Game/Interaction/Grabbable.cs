using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
    }
}
