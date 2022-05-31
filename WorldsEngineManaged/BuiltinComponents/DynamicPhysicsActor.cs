using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using WorldsEngine.ComponentMeta;
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

    public class RigidBody : BuiltinComponent
    {
        [DllImport(Engine.NativeModule)]
        private static extern int dynamicpa_getShapeCount(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getShape(IntPtr registryPtr, uint entityId, int shapeIndex, ref PhysicsShapeInternal psi);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setShapeCount(IntPtr registryPtr, uint entityId, int shapeCount);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setShape(IntPtr registryPtr, uint entityId, int shapeIndex, ref PhysicsShapeInternal psi);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_updateShapes(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_addForce(IntPtr registryPtr, uint entityId, Vector3 force, int forceMode);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_addTorque(IntPtr registryPtr, uint entityId, Vector3 force, int forceMode);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_addForceAtPosition(IntPtr registryPtr, uint entityId, Vector3 force, Vector3 pos, int forceMode);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getPose(IntPtr registryPtr, uint entityId, ref Transform pose);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setPose(IntPtr registryPtr, uint entityId, ref Transform pose);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getLinearVelocity(IntPtr registryPtr, uint entityId, ref Vector3 velocity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setLinearVelocity(IntPtr registryPtr, uint entityId, Vector3 vel);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getAngularVelocity(IntPtr registryPtr, uint entityId, ref Vector3 velocity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setAngularVelocity(IntPtr registryPtr, uint entityId, Vector3 vel);

        [DllImport(Engine.NativeModule)]
        private static extern float dynamicpa_getMass(IntPtr registryPtr, uint entityId);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setMass(IntPtr registryPtr, uint entityId, float mass);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getCenterOfMassLocalPose(IntPtr reg, uint entity, ref Transform pose);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_getMassSpaceInertiaTensor(IntPtr reg, uint entity, ref Vector3 tensor);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setMaxAngularVelocity(IntPtr reg, uint entity, float vel);

        [DllImport(Engine.NativeModule)]
        private static extern float dynamicpa_getMaxAngularVelocity(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setMaxLinearVelocity(IntPtr reg, uint entity, float vel);

        [DllImport(Engine.NativeModule)]
        private static extern float dynamicpa_getMaxLinearVelocity(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool dynamicpa_getKinematic(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setKinematic(IntPtr reg, uint entity, [MarshalAs(UnmanagedType.I1)] bool kinematic);

        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool dynamicpa_getUseContactMod(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setUseContactMod(IntPtr reg, uint entity, [MarshalAs(UnmanagedType.I1)] bool useContactMod);

        [DllImport(Engine.NativeModule)]
        private static extern float dynamicpa_getContactOffset(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setContactOffset(IntPtr reg, uint entity, float val);

        [DllImport(Engine.NativeModule)]
        private static extern float dynamicpa_getSleepThreshold(IntPtr reg, uint entity);

        [DllImport(Engine.NativeModule)]
        private static extern void dynamicpa_setSleepThreshold(IntPtr reg, uint entity, float val);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("Dynamic Physics Actor")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        public Transform Pose
        {
            get
            {
                if (!Registry.Valid(new Entity(entityId)))
                    throw new InvalidEntityException();

                if (!Metadata.ExistsOn(new Entity(entityId)))
                    throw new ComponentDestroyedException();

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

            set => dynamicpa_setLinearVelocity(regPtr, entityId, value);
        }

        public Vector3 AngularVelocity
        {
            get
            {
                Vector3 velocity = new Vector3();
                dynamicpa_getAngularVelocity(regPtr, entityId, ref velocity);
                return velocity;
            }

            set => dynamicpa_setAngularVelocity(regPtr, entityId, value);
        }

        public float Mass
        {
            get => dynamicpa_getMass(regPtr, entityId);
            set => dynamicpa_setMass(regPtr, entityId, value);
        }

        public Transform CenterOfMassLocalPose
        {
            get
            {
                Transform t = new Transform();
                dynamicpa_getCenterOfMassLocalPose(regPtr, entityId, ref t);

                return t;
            }
        }

        public Transform WorldSpaceCenterOfMass => CenterOfMassLocalPose.TransformBy(Pose);

        /// <summary>
        /// The diagonal inertia tensor in mass space.
        /// </summary>
        public Vector3 InertiaTensor
        {
            get
            {
                Vector3 it = new Vector3();
                dynamicpa_getMassSpaceInertiaTensor(regPtr, entityId, ref it);
                return it;
            }
        }

        public float MaxAngularVelocity
        {
            get => dynamicpa_getMaxAngularVelocity(regPtr, entityId);
            set => dynamicpa_setMaxAngularVelocity(regPtr, entityId, value);
        }

        public float MaxLinearVelocity
        {
            get => dynamicpa_getMaxLinearVelocity(regPtr, entityId);
            set => dynamicpa_setMaxLinearVelocity(regPtr, entityId, value);
        }

        public bool Kinematic
        {
            get => dynamicpa_getKinematic(regPtr, entityId);
            set => dynamicpa_setKinematic(regPtr, entityId, value);
        }

        public bool UseContactMod
        {
            get => dynamicpa_getUseContactMod(regPtr, entityId);

            set
            {
                dynamicpa_setUseContactMod(regPtr, entityId, value);
                ForceShapeUpdate();
            }
        }

        public float ContactOffset
        {
            get => dynamicpa_getContactOffset(regPtr, entityId);

            set
            {
                dynamicpa_setContactOffset(regPtr, entityId, value);
                ForceShapeUpdate();
            }
        }

        public float SleepThreshold
        {
            get => dynamicpa_getSleepThreshold(regPtr, entityId);
            set => dynamicpa_setSleepThreshold(regPtr, entityId, value);
        }

        internal RigidBody(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
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

        public void SetPhysicsShapes(IList<PhysicsShape> physicsShapes)
        {
            dynamicpa_setShapeCount(regPtr, entityId, physicsShapes.Count);

            for (int i = 0; i < physicsShapes.Count; i++)
            {
                PhysicsShapeInternal internalShape = physicsShapes[i].ToInternal();
                dynamicpa_setShape(regPtr, entityId, i, ref internalShape);
            }

            dynamicpa_updateShapes(regPtr, entityId);
        }

        public void ForceShapeUpdate()
        {
            dynamicpa_updateShapes(regPtr, entityId);
        }

        public void AddForce(Vector3 force, ForceMode forceMode = ForceMode.Force)
        {
            if (force.HasNaNComponent)
            {
                Log.Error("Tried to add force with a NaN component!");
                return;
            }

            dynamicpa_addForce(regPtr, entityId, force, (int)forceMode);
        }

        public void AddTorque(Vector3 torque, ForceMode forceMode = ForceMode.Force)
        {
            if (torque.HasNaNComponent)
            {
                Log.Error("Tried to add torque with a NaN component!");
                return;
            }

            dynamicpa_addTorque(regPtr, entityId, torque, (int)forceMode);
        }

        public void AddForceAtPosition(Vector3 force, Vector3 position, ForceMode forceMode = ForceMode.Force)
        {
            dynamicpa_addForceAtPosition(regPtr, entityId, force, position, (int)forceMode);
        }
    }
}
