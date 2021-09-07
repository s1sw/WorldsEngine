using System.Linq;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;
using WorldsEngine.Util;

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

            uint overlappedCount = Physics.OverlapSphereMultiple(dpa.Pose.TransformPoint(new Vector3(0.0f, 0.0f, 0.03f)), 0.5f, MaxOverlaps, overlapped);

            for (int i = 0; i < overlappedCount; i++)
            {
                if (!Registry.HasComponent<Grabbable>(overlapped[i])) continue;

                Grab(overlapped[i]);
                return;
            }
        }

        private void AddShapeTensor(PhysicsShape shape, Transform offset, InertiaTensorComputer itComp)
        {
            InertiaTensorComputer shapeComp = new InertiaTensorComputer();
            switch (shape.type)
            {
                case PhysicsShapeType.Sphere:
                    SpherePhysicsShape sphereShape = (SpherePhysicsShape)shape;
                    shapeComp.SetSphere(sphereShape.radius * offset.Scale.ComponentMean);
                    break;
                case PhysicsShapeType.Box:
                    BoxPhysicsShape boxShape = (BoxPhysicsShape)shape;
                    shapeComp.SetBox(boxShape.halfExtents * offset.Scale);

                    break;
            }

            shapeComp.Rotate(offset.Rotation.ToMat3x3());
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

            Transform gripToWorldSpace = grabbedDpa.Pose;
            foreach (var physShape in handShapes)
            {
                AddShapeTensor(physShape, new Transform(physShape.position, physShape.rotation), itComp);
            }

            //InertiaTensorComputer grabbedComp = new();

            foreach (var physShape in grabbedShapes)
            {
                Transform shapeTransform = new Transform(physShape.position, physShape.rotation);
                shapeTransform = gripTransform.TransformBy(shapeTransform);
                shapeTransform.Scale = grabbedTransform.Scale;
                AddShapeTensor(physShape, shapeTransform, itComp);
            }

            itComp.ScaleDensity((grabbedDpa.Mass + dpa.Mass) / itComp.Mass);

            return itComp.Inertia;
        }

        private void Grab(Entity grab)
        {
            Grabbable grabbable = Registry.GetComponent<Grabbable>(grab);
            var physHand = Registry.GetComponent<PhysHand>(Entity);

            Transform handTransform = Registry.GetTransform(Entity);
            Transform grabbingTransform = Registry.GetTransform(grab);

            Transform relativeTransform = grabbingTransform.TransformByInverse(handTransform);

            D6Joint d6 = Registry.AddComponent<D6Joint>(Entity);
            d6.SetAllAxisMotion(D6Motion.Locked);

            GrippedEntity = grab;

            // If it doesn't have grips, just use the current position
            if (grabbable.grips.Count == 0)
            {
                Transform gripTransform = Registry.GetComponent<DynamicPhysicsActor>(grab).Pose.TransformByInverse(handTransform);
                d6.LocalPose = relativeTransform;
                d6.Target = grab;

                physHand.UseOverrideTensor = true;
                physHand.OverrideTensor = CalculateCombinedTensor(grab, gripTransform);
                return;
            }

            GripHand thisHand = IsRightHand ? GripHand.Right : GripHand.Left;

            // Select an appropriate grip and use it
            Grip g = grabbable.grips
                .Where((Grip g) => g.CanAttach && (g.Hand == GripHand.Both || g.Hand == thisHand))
                .OrderByDescending((Grip g) => g.CalculateGripScore(grabbingTransform, handTransform))
                .First();

            if (g == null)
                return;

            if (g.rotation.LengthSquared < 0.9f)
                g.rotation = Quaternion.Identity;

            d6.TargetLocalPose = new Transform(g.position, g.rotation);

            Mat3x3 combinedTensor = CalculateCombinedTensor(grab, new Transform(-g.position, g.rotation.Inverse));

            physHand.UseOverrideTensor = true;
            physHand.OverrideTensor = combinedTensor;
            d6.Target = grab;
        }

        private void Release()
        {
            Logger.Log("Release");
            Registry.RemoveComponent<D6Joint>(Entity);
            GrippedEntity = Entity.Null;
            var physHand = Registry.GetComponent<PhysHand>(Entity);
            physHand.UseOverrideTensor = false;
        }
    }
}
