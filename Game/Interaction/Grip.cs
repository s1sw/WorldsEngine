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
        Cylinder,
        Sphere
    }

    public class Grip
    {
        public GripHand Hand;
        public GripType Type;

        public Vector3 position;
        public Quaternion rotation;

        public Vector3 BoxExtents;
        public float SphereRadius;

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

        public float CalculateGripScore(Transform obj, Transform hand, bool isRightHand)
        {
            hand.Position += hand.Rotation * new Vector3(0.0f, 0.0f, 0.04f);
            Transform attachTransform = GetAttachTransform(hand, obj, isRightHand);
            float linearScore = hand.Position.DistanceTo(obj.TransformPoint(attachTransform.Position));

            if (Type == GripType.Box)
            {
                Transform handInLocalSpace = hand.TransformByInverse(obj);

                linearScore = handInLocalSpace.Position.DistanceTo(GetAttachPointForBoxGrip(handInLocalSpace.Position));
            }
            else if (Type == GripType.Sphere)
            {
                Transform handInLocalSpace = hand.TransformByInverse(obj);

                linearScore = handInLocalSpace.Position.DistanceTo(GetAttachPointForBoxGrip(handInLocalSpace.Position));
            }

            float angularScore = Quaternion.Dot(hand.Rotation.SingleCover, (obj.Rotation * rotation).SingleCover);

            return 5.0f - linearScore;
        }

        public Transform GetAttachTransform(Transform handTransform, Transform objTransform, bool isRightHand)
        {
            handTransform.Position += handTransform.Rotation * new Vector3(0.0f, 0.0f, 0.03f);
            Transform handInLocalSpace = handTransform.TransformByInverse(objTransform);
            if (Type == GripType.Box)
            {
                Vector3 position = GetAttachPointForBoxGrip(handInLocalSpace.Position);
                Vector3 normal = GetNormalForBoxGrip(handInLocalSpace.Position);

                // Get the hand's rotation around the normal axis and apply that to make the grip smoother
                Quaternion handRotAroundNormal = handInLocalSpace.Rotation.DecomposeTwist(normal);

                Quaternion rotation = handRotAroundNormal * Quaternion.FromTo(isRightHand ? Vector3.Right : Vector3.Left, normal);

                return new Transform(position, rotation);
            }
            else if (Type == GripType.Sphere)
            {
                Vector3 position = GetAttachPointForSphereGrip(handInLocalSpace.Position);
                Vector3 normal = GetNormalForSphereGrip(handInLocalSpace.Position);
                
                // Get the hand's rotation around the normal axis and apply that to make the grip smoother
                Quaternion handRotAroundNormal = handInLocalSpace.Rotation.DecomposeTwist(normal);
                
                Quaternion rotation = handRotAroundNormal * Quaternion.FromTo(isRightHand ? Vector3.Right : Vector3.Left, normal);
                
                return new Transform(position, rotation);
            }

            return new Transform(position, rotation);
        }

        // Get the attach point in local space.
        private Vector3 GetAttachPointForBoxGrip(Vector3 p)
        {
            Vector3 d = p - position;

            // Start result at center of box; make steps from there
            Vector3 q = position;

            Vector3 xAxis = rotation * Vector3.Left;
            Vector3 yAxis = rotation * Vector3.Up;
            Vector3 zAxis = rotation * Vector3.Forward;

            float xDist = Vector3.Dot(xAxis, d);
            xDist = MathFX.Clamp(xDist, -BoxExtents.x, BoxExtents.x);

            float yDist = Vector3.Dot(yAxis, d);
            yDist = MathFX.Clamp(yDist, -BoxExtents.y, BoxExtents.y);

            float zDist = Vector3.Dot(zAxis, d);
            zDist = MathFX.Clamp(zDist, -BoxExtents.z, BoxExtents.z);

            return q + (xAxis * xDist) + (yAxis * yDist) + (zAxis * zDist);
        }

        private Vector3 GetNormalForBoxGrip(Vector3 p)
        {
            Vector3 d = p - position;

            Vector3 xAxis = rotation * Vector3.Left;
            Vector3 yAxis = rotation * Vector3.Up;
            Vector3 zAxis = rotation * Vector3.Forward;

            float xDist = Vector3.Dot(xAxis, d);
            xDist = MathFX.Clamp(xDist, -BoxExtents.x, BoxExtents.x);

            float yDist = Vector3.Dot(yAxis, d);
            yDist = MathFX.Clamp(yDist, -BoxExtents.y, BoxExtents.y);

            float zDist = Vector3.Dot(zAxis, d);
            zDist = MathFX.Clamp(zDist, -BoxExtents.z, BoxExtents.z);

            if (MathF.Abs(xDist) >= MathF.Abs(yDist) && MathF.Abs(xDist) >= MathF.Abs(zDist))
            {
                return xAxis * MathF.Sign(xDist);
            }

            if (MathF.Abs(yDist) >= MathF.Abs(xDist) && MathF.Abs(yDist) >= MathF.Abs(zDist))
            {
                return yAxis * MathF.Sign(yDist);
            }

            if (MathF.Abs(zDist) >= MathF.Abs(xDist) && MathF.Abs(zDist) >= MathF.Abs(yDist))
            {
                return zAxis * MathF.Sign(zDist);
            }

            return yAxis;
        }

        private Vector3 GetAttachPointForSphereGrip(Vector3 p)
        {
            Vector3 d = p - position;
            d.Normalize();
            return position + d * SphereRadius;
        }

        private Vector3 GetNormalForSphereGrip(Vector3 p)
        {
            return (p - position).Normalized;
        }
    }
}
