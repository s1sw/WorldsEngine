using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public enum ForceMode
    {
        Force,
        Impulse,
        VelocityChange,
        Acceleration
    }

    public class DynamicPhysicsActor : BuiltinComponent
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern int dynamicpa_getShapeCount(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_getShape(IntPtr registryPtr, uint entityId, int shapeIndex, ref PhysicsShapeInternal psi);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_setShapeCount(IntPtr registryPtr, uint entityId, int shapeCount);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_setShape(IntPtr registryPtr, uint entityId, int shapeIndex, ref PhysicsShapeInternal psi);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_addForce(IntPtr registryPtr, uint entityId, Vector3 force, int forceMode);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_addTorque(IntPtr registryPtr, uint entityId, Vector3 force, int forceMode);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_getPose(IntPtr registryPtr, uint entityId, ref Transform pose);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_setPose(IntPtr registryPtr, uint entityId, ref Transform pose);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_getLinearVelocity(IntPtr registryPtr, uint entityId, ref Vector3 velocity);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern float dynamicpa_getMass(IntPtr registryPtr, uint entityId);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void dynamicpa_setMass(IntPtr registryPtr, uint entityId, float mass);

        private readonly IntPtr regPtr;
        private readonly uint entityId;

        public Transform Pose
        {
            get
            {
                Transform pose = new Transform();
                dynamicpa_getPose(regPtr, entityId, ref pose);

                return pose;
            }

            set
            {
                dynamicpa_setPose(regPtr, entityId, ref value);
            }
        }

        public Vector3 Velocity
        {
            get
            {
                Vector3 velocity = new Vector3();
                dynamicpa_getLinearVelocity(regPtr, entityId, ref velocity);

                return velocity;
            }
        }

        public float Mass
        {
            get => dynamicpa_getMass(regPtr, entityId);
            set => dynamicpa_setMass(regPtr, entityId, value);
        }

        internal DynamicPhysicsActor(IntPtr regPtr, uint entityId)
        {
            this.regPtr = regPtr;
            this.entityId = entityId;
        }

        public List<PhysicsShape> GetPhysicsShapes()
        {
            List<PhysicsShape> shapes = new List<PhysicsShape>();

            int shapeCount = dynamicpa_getShapeCount(regPtr, entityId);

            for (int i = 0; i < shapeCount; i++)
            {
                PhysicsShapeInternal internalShape = new PhysicsShapeInternal();
                dynamicpa_getShape(regPtr, entityId, i, ref internalShape);
                shapes.Add(PhysicsShape.FromInternal(internalShape));
            }

            return shapes;
        }

        public void SetPhysicsShapes(List<PhysicsShape> physicsShapes)
        {
            dynamicpa_setShapeCount(regPtr, entityId, physicsShapes.Count);

            for (int i = 0; i < physicsShapes.Count; i++)
            {
                PhysicsShapeInternal internalShape = physicsShapes[i].ToInternal();
                dynamicpa_setShape(regPtr, entityId, i, ref internalShape);
            }
        }

        public void AddForce(Vector3 force, ForceMode forceMode = ForceMode.Force)
        {
            dynamicpa_addForce(regPtr, entityId, force, (int)forceMode);
        }

        public void AddTorque(Vector3 torque, ForceMode forceMode = ForceMode.Force)
        {
            dynamicpa_addTorque(regPtr, entityId, torque, (int)forceMode);
        }
    }
}
