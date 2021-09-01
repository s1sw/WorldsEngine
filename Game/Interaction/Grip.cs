using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace Game.Interaction
{
    public enum GripHand
    {
        Left,
        Right,
        Both
    }

    public enum GripType
    {
        Manual,
        Box,
        Cylinder
    }

    public class Grip
    {
        public GripHand Hand;
        public GripType Type;

        public Vector3 position;
        public Quaternion rotation;

        public bool Exclusive;

        public bool InUse { get; private set; }
        public int CurrentlyAttached = 0;

        internal void Attach()
        {
            if (Exclusive && InUse) throw new InvalidOperationException("Can't attach to a grip that's in use");

            InUse = true;
            CurrentlyAttached++;
        }

        internal void Detach()
        {
            CurrentlyAttached--;

            if (CurrentlyAttached == 0)
                InUse = false;
        }
    }
}
