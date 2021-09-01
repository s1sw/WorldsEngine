using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game.Interaction
{
    [Component]
    public class HandGrab : IThinkingComponent
    {
        public bool IsRightHand = false;

        private static Entity Entity;

        public Entity GrippedEntity { get; private set; } = Entity.Null;

        public int GripIndex { get; private set; }

        private VRAction _grabAction;

        public void Think(Entity entity)
        {
            Entity = entity;
            if (VR.Enabled)
            {
                if (_grabAction == null)
                {
                    _grabAction = new VRAction(IsRightHand ? "/actions/main/in/GrabR" : "/actions/main/in/GrabL");
                }

                if (_grabAction.Pressed && GrippedEntity.IsNull)
                    GrabNearby();

                if (_grabAction.Released && !GrippedEntity.IsNull)
                    Release();
            }
            else
            {
                MouseButton mouseButton = IsRightHand ? MouseButton.Right : MouseButton.Left;

                if (Mouse.ButtonPressed(mouseButton) && GrippedEntity.IsNull)
                    GrabNearby();

                if (Mouse.ButtonReleased(mouseButton) && !GrippedEntity.IsNull)
                    Release();
            }
        }

        private void GrabNearby()
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);

            const int MaxOverlaps = 10;
            Entity[] overlapped = new Entity[MaxOverlaps];

            uint overlappedCount = Physics.OverlapSphereMultiple(dpa.Pose.TransformPoint(new Vector3(0.0f, 0.0f, 0.03f)), 0.1f, MaxOverlaps, overlapped);

            for (int i = 0; i < overlappedCount; i++)
            {
                if (!Registry.HasComponent<Grabbable>(overlapped[i])) continue;

                Grab(overlapped[i]);
                return;
            }
        }

        private void Grab(Entity grab)
        {
            Grabbable grabbable = Registry.GetComponent<Grabbable>(grab);

            Transform handTransform = Registry.GetTransform(Entity);
            Transform grabbingTransform = Registry.GetTransform(grab);

            Transform relativeTransform = grabbingTransform.TransformByInverse(handTransform);

            D6Joint d6 = Registry.AddComponent<D6Joint>(Entity);
            d6.SetAllAxisMotion(D6Motion.Locked);
            d6.LocalPose = relativeTransform;
            d6.Target = grab;

            GrippedEntity = grab;

            // If it doesn't have grips, just use the current position
            if (grabbable.grips.Count == 0)
            {
                return;
            }

            // Select an appropriate grip and use it

        }

        private void Release()
        {
            Logger.Log("Release");
            Registry.RemoveComponent<D6Joint>(Entity);
            GrippedEntity = Entity.Null;
        }
    }
}
