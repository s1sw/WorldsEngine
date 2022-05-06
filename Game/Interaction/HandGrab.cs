using System.Linq;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;
using WorldsEngine.Util;
using System.Threading.Tasks;
using Game.Util;
using ImGuiNET;
using System;
using Game.Player;

namespace Game.Interaction;

[Component]
public class HandGrab : Component
{
    public bool IsRightHand = false;

    public Entity GrippedEntity { get; private set; } = Entity.Null;

    public Grip CurrentGrip { get; private set; } = null;

    public static event Action<Entity, AttachedHandFlags> OnGrabEntity;
    public static event Action<Entity, AttachedHandFlags> OnReleaseEntity;

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
            if (!Registry.Valid(GrippedEntity))
            {
                Release();
                return;
            }

            var physHand = Entity.GetComponent<PhysHand>();
            //Mat3x3 combinedTensor = CalculateCombinedTensor(GrippedEntity);

            //physHand.UseOverrideTensor = true;
            //physHand.OverrideTensor = combinedTensor;

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

    public void GrabNearby()
    {
        var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);

        const int MaxOverlaps = 32;
        Span<Entity> overlaps = stackalloc Entity[MaxOverlaps];
        uint overlappedCount = Physics.OverlapSphereMultiple(dpa.Pose.TransformPoint(new Vector3(0.0f, 0.0f, 0.03f)), 0.2f, MaxOverlaps, overlaps);

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

    private Mat3x3 CalculateCombinedTensor(Entity grabbed)
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
        handComp.Translate(dpa.Pose.Position);
        handComp.Rotate(dpa.Pose.Rotation);

        InertiaTensorComputer grabbedComp = new();
        foreach (var physShape in grabbedShapes)
        {
            Transform shapeTransform = new(physShape.position, physShape.rotation);
            shapeTransform.Scale = grabbedTransform.Scale;

            AddShapeTensor(physShape, shapeTransform, grabbedComp);
        }

        grabbedComp.Translate(grabbedDpa.Pose.Position);
        grabbedComp.Rotate(grabbedDpa.Pose.Rotation);

        handComp.ScaleDensity(dpa.Mass / handComp.Mass);
        grabbedComp.ScaleDensity(grabbedDpa.Mass / grabbedComp.Mass);

        itComp.Add(handComp);
        itComp.Add(grabbedComp);

        itComp.Rotate(dpa.Pose.Rotation.Inverse);
        itComp.Translate(-dpa.Pose.Position);

        return itComp.InertiaTensor;
    }

    private Mat3x3 CalculateCombinedTensorOld(Entity grabbed, Transform gripTransform)
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
        if (_bringingTowards) return;
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
            physHand.OverrideTensor = CalculateCombinedTensor(grab);
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
        // THIS IS ALL MADNESS
        // AAAAAAa.
        Transform handOffset = new(new Vector3(0.025f * (IsRightHand ? -1f : 1f), -0.0f, 0.0f), Quaternion.Identity);

        if (CurrentGrip.Type == GripType.Manual) handOffset.Position = Vector3.Zero;

        var gripInObjectSpace = CurrentGrip.GetAttachTransform(handDpa.Pose, grabbedDpa.Pose, IsRightHand);
        Transform gripInWorldSpace = new();

        float timeWaiting = 0.0f;
        while (_bringingTowards)
        {
            await Awaitables.NextSimulationTick;

            timeWaiting += Time.DeltaTime;

            //gripInObjectSpace = CurrentGrip.GetAttachTransform(handDpa.Pose, grabbedDpa.Pose, IsRightHand);

            gripInWorldSpace = gripInObjectSpace.TransformBy(grabbedDpa.Pose);
            gripInWorldSpace.Position += gripInWorldSpace.Rotation * handOffset.Position;
            physHand.OverrideTarget = gripInWorldSpace;
            DebugShapes.DrawSphere(gripInWorldSpace.Position, 0.1f, gripInWorldSpace.Rotation, WorldsEngine.Util.Colors.White);

            float distance = handDpa.Pose.Position.DistanceTo(gripInWorldSpace.Position);

            float acceptableDistance = 0.05f + MathF.Max(timeWaiting - 0.2f, 0.0f);
            if (distance < acceptableDistance)
                break;

            // Too far, let's give up
            //if (distance > 1.0f)
            //{
            //    Release();
            //    break;
            //}
        }

        if (!_bringingTowards)
        {
            physHand.OverrideTarget = null;
            return;
        }

        var finalOsGrip = gripInWorldSpace.TransformByInverse(grabbedDpa.Pose);

        Mat3x3 combinedTensor = CalculateCombinedTensorOld(grabbed, finalOsGrip);

        physHand.UseOverrideTensor = true;
        physHand.OverrideTensor = combinedTensor;
        physHand.OverrideTarget = null;

        D6Joint d6 = Registry.GetComponent<D6Joint>(Entity);
        d6.TargetLocalPose = finalOsGrip;
        d6.SetAllAxisMotion(D6Motion.Locked);
        //d6.SetAllAxisMotion(D6Motion.Free);
        //d6.SetAllAngularAxisMotion(D6Motion.Free);
        //D6JointDrive drive = new(25f, 5f, float.MaxValue, false);
        //D6JointDrive posDrive = new(5000f, 200f, float.MaxValue, false);
        //d6.SetDrive(D6Drive.Swing, drive);
        //d6.SetDrive(D6Drive.Twist, drive);
        //d6.SetDrive(D6Drive.X, posDrive);
        //d6.SetDrive(D6Drive.Y, posDrive);
        //d6.SetDrive(D6Drive.Z, posDrive);
        Entity.GetComponent<PhysHand>().TorqueScale = CurrentGrip.TorqueScale;
        OnGrabEntity?.Invoke(grabbed, IsRightHand ? AttachedHandFlags.Right : AttachedHandFlags.Left);
        grabbed.GetComponent<Grabbable>().InvokeOnGrabbed(CurrentGrip);
    }

    private void Release()
    {
        if (CurrentGrip != null)
            CurrentGrip.Detach(IsRightHand ? AttachedHandFlags.Right : AttachedHandFlags.Left);

        if (GrippedEntity.IsValid && Registry.TryGetComponent<DynamicPhysicsActor>(GrippedEntity, out var dpa))
        {
            Vector3 linVel = dpa.Velocity;
            Vector3 angVel = dpa.AngularVelocity;
            dpa.Kinematic = true;
            dpa.Kinematic = false;
            dpa.Velocity = linVel;
            dpa.AngularVelocity = angVel;
        }

        Registry.RemoveComponent<D6Joint>(Entity);

        Entity gripped = GrippedEntity;
        GrippedEntity = Entity.Null;
        _bringingTowards = false;

        var physHand = Registry.GetComponent<PhysHand>(Entity);
        physHand.UseOverrideTensor = false;
        physHand.TorqueScale = new(1.0f);

        if (gripped.IsValid)
        {
            OnReleaseEntity?.Invoke(gripped, IsRightHand ? AttachedHandFlags.Right : AttachedHandFlags.Left);
            gripped.GetComponent<Grabbable>().InvokeOnReleased(CurrentGrip);
        }
        CurrentGrip = null;
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
