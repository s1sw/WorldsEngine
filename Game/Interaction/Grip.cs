using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
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

        public bool InUse => CurrentlyAttached > 0;
        public bool CanAttach => !Exclusive || !InUse;
        public int CurrentlyAttached { get; private set; }
        public AttachedHandFlags CurrentlyAttachedHand { get; private set; }

        internal void Attach(AttachedHandFlags hand)
        {
            if (Exclusive && InUse) throw new InvalidOperationException("Can't attach to a grip that's in use");

            CurrentlyAttached++;
            CurrentlyAttachedHand |= hand;
        }

        internal void Detach(AttachedHandFlags hand)
        {
            CurrentlyAttached--;

            CurrentlyAttachedHand &= ~hand;
        }

        public float CalculateGripScore(Transform obj, Transform hand)
        {
            float linearScore = 1.0f / hand.Position.DistanceTo(obj.TransformPoint(position));
            float angularScore = Quaternion.Dot(hand.Rotation.SingleCover, (rotation * obj.Rotation).SingleCover);

            return linearScore;
        }
    }
}
