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

            uint overlappedCount = Physics.OverlapSphereMultiple(dpa.Pose.TransformPoint(new Vector3(0.0f, 0.0f, 0.03f)), 0.1f, MaxOverlaps, overlapped);

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
                    float m = 8.0f * (boxShape.halfExtents.x * boxShape.halfExtents.y * boxShape.halfExtents.z);
                    Logger.Log($"s: {(1.0f / 3.0f) * m}");
                    Logger.Log($"he: {boxShape.halfExtents}");

                    break;
            }
            Logger.Log($"shape type {shape.type}");

            Mat3x3 phs = itComp.Inertia;

            Logger.Log($"Shape tensor before transform: {phs[0]}, {phs[1]}, {phs[2]}");
            shapeComp.Rotate(offset.Rotation.ToMat3x3());
            shapeComp.Translate(offset.Position);
            phs = itComp.Inertia;
            Logger.Log($"Shape tensor after transform: {phs[0]}, {phs[1]}, {phs[2]}");

            itComp.Add(shapeComp);
        }

        private Mat3x3 CalculateCombinedTensor(Entity grabbed)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);
            var grabbedDpa = Registry.GetComponent<DynamicPhysicsActor>(grabbed);

            InertiaTensorComputer itComp = new InertiaTensorComputer();

            var handShapes = dpa.GetPhysicsShapes();
            var grabbedShapes = grabbedDpa.GetPhysicsShapes();

            foreach (var physShape in handShapes)
            {
                AddShapeTensor(physShape, new Transform() { Rotation = Quaternion.Identity, Scale = Vector3.One }, itComp);
            }
            Mat3x3 phs = itComp.Inertia;

            Logger.Log($"Tensor after adding hand shapes: {phs[0]}, {phs[1]}, {phs[2]}");

            foreach (var physShape in grabbedShapes)
            {
                AddShapeTensor(physShape, grabbedDpa.Pose.TransformByInverse(dpa.Pose), itComp);
            }

            return itComp.Inertia;
        }

        private void Grab(Entity grab)
        {
            Grabbable grabbable = Registry.GetComponent<Grabbable>(grab);

            Transform handTransform = Registry.GetTransform(Entity);
            Transform grabbingTransform = Registry.GetTransform(grab);

            Transform relativeTransform = grabbingTransform.TransformByInverse(handTransform);

            D6Joint d6 = Registry.AddComponent<D6Joint>(Entity);
            d6.SetAllAxisMotion(D6Motion.Locked);
            d6.Target = grab;

            GrippedEntity = grab;

            // If it doesn't have grips, just use the current position
            if (grabbable.grips.Count == 0)
            {
                d6.LocalPose = relativeTransform;
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
            var physHand = Registry.GetComponent<PhysHand>(Entity);

            Mat3x3 combinedTensor = CalculateCombinedTensor(grab);
            Logger.Log($"Inertia tensor: {combinedTensor[0]}, {combinedTensor[1]}, {combinedTensor[2]}");

            physHand.UseOverrideTensor = true;
            physHand.OverrideTensor = combinedTensor;
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
