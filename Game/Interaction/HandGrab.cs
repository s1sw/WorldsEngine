using System.Linq;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;
using WorldsEngine.Util;
using System.Threading.Tasks;
using Game.Util;
using ImGuiNET;
using System;

namespace Game.Interaction
{
    [Component]
    public class HandGrab : Component
    {
        public bool IsRightHand = false;

        public Entity GrippedEntity { get; private set; } = Entity.Null;

        public Grip CurrentGrip { get; private set; } = null;

        private VRAction _grabAction;
        private VRAction _triggerAction;
        private bool _bringingTowards = false;
        private float _lastControllerTrigVal = 0.0f;

        public void Update()
        {
            if (VR.Enabled)
            {
                if (_grabAction == null)
                {
                    _grabAction = new VRAction(IsRightHand ? "/actions/main/in/GrabR" : "/actions/main/in/GrabL");
                    _triggerAction = new VRAction(IsRightHand ? "/actions/main/in/TriggerR" : "/actions/main/in/TriggerL");
                }

                if (_grabAction.Pressed && GrippedEntity.IsNull)
                    GrabNearby();

                if (_grabAction.Released && !GrippedEntity.IsNull)
                    Release();
            }
            else
            {
                MouseButton mouseButton = IsRightHand ? MouseButton.Right : MouseButton.Left;
                KeyCode key = IsRightHand ? KeyCode.E : KeyCode.Q;
                ControllerButton cButton = IsRightHand ? ControllerButton.RightShoulder : ControllerButton.LeftShoulder;

                if ((Keyboard.KeyPressed(key) || Controller.ButtonPressed(cButton)) && !GrippedEntity.IsNull)
                    Release();
                else if ((Keyboard.KeyPressed(key) || Controller.ButtonPressed(cButton)) && GrippedEntity.IsNull)
                    GrabNearby();
            }

            if (!GrippedEntity.IsNull)
            {
                var grabbable = Registry.GetComponent<Grabbable>(GrippedEntity);

                if (!VR.Enabled)
                {
                    KeyCode keycode = IsRightHand ? KeyCode.E : KeyCode.Q;
                    MouseButton mouseButton = IsRightHand ? MouseButton.Right : MouseButton.Left;
                    ControllerAxis axis = IsRightHand ? ControllerAxis.TriggerRight : ControllerAxis.TriggerLeft;
                    float axisVal = Controller.AxisValue(axis);

                    bool axisPressed = axisVal > 0.75f && _lastControllerTrigVal < 0.75f;
                    bool axisReleased = _lastControllerTrigVal > 0.75f && axisVal < 0.75f;

                    _lastControllerTrigVal = axisVal;

                    grabbable.RunEvents(
                        Mouse.ButtonPressed(mouseButton) || axisPressed,
                        Mouse.ButtonReleased(mouseButton) || axisReleased,
                        Mouse.ButtonHeld(mouseButton) || axisVal > 0.75f,
                        GrippedEntity
                    );
                }
                else
                {
                    grabbable.RunEvents(
                        _triggerAction.Pressed,
                        _triggerAction.Released,
                        _triggerAction.Held,
                        GrippedEntity
                    );
                }
            }
        }

        private void GrabNearby()
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);

            const int MaxOverlaps = 32;
            Span<Entity> overlaps = stackalloc Entity[MaxOverlaps];
            uint overlappedCount = Physics.OverlapSphereMultiple(dpa.Pose.TransformPoint(new Vector3(0.0f, 0.0f, 0.03f)), 0.5f, MaxOverlaps, overlaps);

            Entity? bestGrabbable = null;
            float bestGrabbableScore = -100000.0f;
            for (int i = 0; i < overlappedCount; i++)
            {
                Entity candidate = overlaps[i];
                if (!candidate.HasComponent<Grabbable>()) continue;
                var grabbable = candidate.GetComponent<Grabbable>();
                var candidateDpa = candidate.GetComponent<DynamicPhysicsActor>();

                float maxGripScore = 0.0f;
                for (int j = 0; j < grabbable.grips.Count; j++)
                {
                    float score = grabbable.grips[j].CalculateGripScore(candidateDpa.Pose, dpa.Pose, IsRightHand);

                    if (score > maxGripScore)
                        maxGripScore = score;
                }

                if (maxGripScore > bestGrabbableScore)
                {
                    bestGrabbableScore = maxGripScore;
                    bestGrabbable = candidate;
                }
            }

            if (bestGrabbable != null)
                Grab(bestGrabbable.Value);
        }

        private void AddShapeTensor(PhysicsShape shape, Transform offset, InertiaTensorComputer itComp)
        {
            InertiaTensorComputer shapeComp = new();
            switch (shape.type)
            {
                case PhysicsShapeType.Sphere:
                    var sphereShape = (SpherePhysicsShape)shape;
                    shapeComp.SetSphere(sphereShape.radius * offset.Scale.ComponentMean);
                    break;
                case PhysicsShapeType.Box:
                    var boxShape = (BoxPhysicsShape)shape;
                    shapeComp.SetBox(boxShape.halfExtents * offset.Scale);
                    break;
                case PhysicsShapeType.Capsule:
                    var capsuleShape = (CapsulePhysicsShape)shape;
                    shapeComp.SetCapsule(InertiaTensorComputer.CapsuleAxis.X, capsuleShape.radius, capsuleShape.height);
                    break;
            }

            shapeComp.Rotate(offset.Rotation);
            shapeComp.Translate(offset.Position);

            itComp.Add(shapeComp);
        }

        private Mat3x3 CalculateCombinedTensor(Entity grabbed, Transform gripTransform)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);
            var grabbedDpa = Registry.GetComponent<DynamicPhysicsActor>(grabbed);

            var transform = Registry.GetTransform(Entity);
            var grabbedTransform = Registry.GetTransform(grabbed);

            InertiaTensorComputer itComp = new();

            var handShapes = dpa.GetPhysicsShapes();
            var grabbedShapes = grabbedDpa.GetPhysicsShapes();

            InertiaTensorComputer handComp = new();
            foreach (var physShape in handShapes)
            {
                AddShapeTensor(physShape, new Transform(physShape.position, physShape.rotation), handComp);
            }

            InertiaTensorComputer grabbedComp = new();
            foreach (var physShape in grabbedShapes)
            {
                Transform shapeTransform = new(physShape.position, physShape.rotation);
                shapeTransform.Scale = grabbedTransform.Scale;

                AddShapeTensor(physShape, shapeTransform, grabbedComp);
            }

            grabbedComp.Rotate(gripTransform.Rotation.Inverse);
            grabbedComp.Translate(-gripTransform.Position);

            handComp.ScaleDensity(dpa.Mass / handComp.Mass);
            grabbedComp.ScaleDensity(grabbedDpa.Mass / grabbedComp.Mass);

            itComp.Add(handComp);
            itComp.Add(grabbedComp);

            return itComp.InertiaTensor;
        }

        private void Grab(Entity grab)
        {
            Grabbable grabbable = Registry.GetComponent<Grabbable>(grab);
            var physHand = Registry.GetComponent<PhysHand>(Entity);

            Transform handTransform = Registry.GetComponent<DynamicPhysicsActor>(Entity).Pose;
            Transform grabbingTransform = Registry.GetComponent<DynamicPhysicsActor>(grab).Pose;

            Transform relativeTransform = grabbingTransform.TransformByInverse(handTransform);

            D6Joint d6 = Registry.AddComponent<D6Joint>(Entity);
            d6.SetAllAxisMotion(D6Motion.Free);

            GrippedEntity = grab;

            // If it doesn't have grips, just use the current position
            if (grabbable.grips.Count == 0)
            {
                Transform gripTransform = Registry.GetComponent<DynamicPhysicsActor>(grab).Pose.TransformByInverse(handTransform);
                d6.LocalPose = relativeTransform;
                d6.Target = grab;

                physHand.UseOverrideTensor = true;
                physHand.OverrideTensor = CalculateCombinedTensor(grab, gripTransform);
                d6.SetAllAxisMotion(D6Motion.Locked);
                return;
            }

            GripHand thisHand = IsRightHand ? GripHand.Right : GripHand.Left;

            // Select an appropriate grip and use it
            var filteredGrips = grabbable.grips
                .Where((Grip g) => g.CanAttach && (g.Hand == GripHand.Both || g.Hand == thisHand));

            Grip g = filteredGrips
                .OrderByDescending((Grip g) => g.CalculateGripScore(grabbingTransform, handTransform, IsRightHand))
                .FirstOrDefault();

            if (g == null)
            {
                Registry.RemoveComponent<D6Joint>(Entity);
                return;
            }

            if (g.rotation.LengthSquared < 0.9f)
                g.rotation = Quaternion.Identity;

            Transform attachTransform = g.GetAttachTransform(handTransform, grabbingTransform, IsRightHand);
            d6.TargetLocalPose = attachTransform;

            d6.Target = grab;

            // If the target is already in contact with the hand, it may continue to collide
            // even though we've added a D6 joint that ignores collisions.
            // We can work around this by toggling the Kinematic property of one of the bodies.
            var handBody = Registry.GetComponent<DynamicPhysicsActor>(Entity);

            Vector3 oldVelocity = handBody.Velocity;
            handBody.Kinematic = true;
            handBody.Kinematic = false;
            handBody.Velocity = oldVelocity;

            g.Attach(IsRightHand ? AttachedHandFlags.Right : AttachedHandFlags.Left);
            CurrentGrip = g;

            _bringingTowards = true;
            BringTowards(grab);
        }

        private async void BringTowards(Entity grabbed)
        {
            var physHand = Registry.GetComponent<PhysHand>(Entity);
            var handDpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);
            var grabbedDpa = Registry.GetComponent<DynamicPhysicsActor>(grabbed);
            var gripInObjectSpace = CurrentGrip.GetAttachTransform(handDpa.Pose, grabbedDpa.Pose, IsRightHand);

            while (_bringingTowards)
            {
                await Task.Delay(10);

                gripInObjectSpace = CurrentGrip.GetAttachTransform(handDpa.Pose, grabbedDpa.Pose, IsRightHand);

                var gripInWorldSpace = gripInObjectSpace.TransformBy(grabbedDpa.Pose);
                physHand.OverrideTarget = gripInWorldSpace;

                float distance = handDpa.Pose.Position.DistanceTo(gripInWorldSpace.Position);

                if (distance < 0.1f)
                    break;

                // Too far, let's give up
                if (distance > 1.0f)
                {
                    Release();
                    break;
                }
            }

            if (!_bringingTowards)
            {
                physHand.OverrideTarget = null;
                return;
            }

            Mat3x3 combinedTensor = CalculateCombinedTensor(grabbed, gripInObjectSpace);

            physHand.UseOverrideTensor = true;
            physHand.OverrideTensor = combinedTensor;
            physHand.OverrideTarget = null;

            D6Joint d6 = Registry.GetComponent<D6Joint>(Entity);
            d6.TargetLocalPose = gripInObjectSpace;
            d6.SetAllAxisMotion(D6Motion.Locked);
        }

        private void Release()
        {
            if (CurrentGrip != null)
                CurrentGrip.Detach(IsRightHand ? AttachedHandFlags.Right : AttachedHandFlags.Left);

            if (Registry.TryGetComponent<DynamicPhysicsActor>(GrippedEntity, out var dpa))
            {
                Vector3 linVel = dpa.Velocity;
                Vector3 angVel = dpa.AngularVelocity;
                dpa.Kinematic = true;
                dpa.Kinematic = false;
                dpa.Velocity = linVel;
                dpa.AngularVelocity = angVel;
            }
            Registry.RemoveComponent<D6Joint>(Entity);
            GrippedEntity = Entity.Null;
            _bringingTowards = false;

            var physHand = Registry.GetComponent<PhysHand>(Entity);
            physHand.UseOverrideTensor = false;
        }
    }

    public class HandGrabSystem : ISystem
    {
        public void OnUpdate()
        {
            foreach (Entity e in Registry.View<HandGrab>())
            {
                e.GetComponent<HandGrab>().Update();
            }
        }
    }
}
