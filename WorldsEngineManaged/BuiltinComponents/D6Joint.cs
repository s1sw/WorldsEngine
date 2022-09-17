using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public enum D6Motion
    {
        Locked,
        Limited,
        Free
    }

    public enum D6Axis
    {
        X,
        Y,
        Z,
        AngularX,
        AngularY,
        AngularZ
    }

    public enum D6Drive
    {
        X,
        Y,
        Z,
        Swing,
        Twist,
        Slerp
    }

    public struct JointLinearLimit
    {
        public float Restitution;
        public float BounceThreshold;
        public float Stiffness;
        public float Damping;

        public float ContactDistance;
        public float Value;

        public JointLinearLimit(float extent)
        {
            Restitution = 0.0f;
            BounceThreshold = 0.0f;
            Stiffness = 0.0f;
            Damping = 0.0f;
            ContactDistance = 0.01f;
            Value = extent;
        }
    }

    public struct JointLinearLimitPair
    {
        public float Restitution;
        public float BounceThreshold;
        public float Stiffness;
        public float Damping;
        public float ContactDistance;

        public float UpperLimit;
        public float LowerLimit;

        public JointLinearLimitPair(float lowerLimit, float upperLimit)
        {
            Restitution = 0.0f;
            BounceThreshold = 0.0f;
            Stiffness = 0.0f;
            Damping = 0.0f;
            ContactDistance = 0.01f;
            LowerLimit = lowerLimit;
            UpperLimit = upperLimit;
        }
    }

    public struct JointAngularLimitPair
    {
        public float Restitution;
        public float BounceThreshold;
        public float Stiffness;
        public float Damping;
        public float ContactDistance;

        public float UpperLimit;
        public float LowerLimit;

        public JointAngularLimitPair(float lowerLimit, float upperLimit)
        {
            Restitution = 0.0f;
            BounceThreshold = 0.0f;
            Stiffness = 0.0f;
            Damping = 0.0f;
            ContactDistance = 0.01f;
            LowerLimit = lowerLimit;
            UpperLimit = upperLimit;
        }
    }

    public struct JointLimitPyramid
    {
        public float Restitution;
        public float BounceThreshold;
        public float Stiffness;
        public float Damping;
        public float ContactDistance;

        public float YAngleMin;
        public float YAngleMax;
        public float ZAngleMin;
        public float ZAngleMax;
    }

    public struct JointLimitCone
    {
        public float Restitution;
        public float BounceThreshold;
        public float Stiffness;
        public float Damping;
        public float ContactDistance;

        public float YAngle;
        public float ZAngle;
    }

    [Flags]
    public enum D6JointDriveFlag : uint
    {
        None = 0,
        Acceleration = 1
    }

    public struct D6JointDrive
    {
        public float Stiffness;
        public float Damping;
        public float ForceLimit;
        public D6JointDriveFlag Flags;

        public D6JointDrive(float stiffness, float damping, float forceLimit = float.MaxValue, bool isAcceleration = false) {
            Stiffness = stiffness;
            Damping = damping;
            ForceLimit = forceLimit;
            Flags = isAcceleration ? D6JointDriveFlag.Acceleration : D6JointDriveFlag.None;
        }
    }

    public class D6Joint : BuiltinComponent
    {
        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setTarget(IntPtr regPtr, uint d6ent, uint target);

        [DllImport(Engine.NativeModule)]
        private static extern uint d6joint_getTarget(IntPtr regPtr, uint d6ent);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setAxisMotion(IntPtr regPtr, uint d6ent, D6Axis axis, D6Motion motion);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_getLocalPose(IntPtr regPtr, uint d6ent, uint actorIndex, out Transform pose);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setLocalPose(IntPtr regPtr, uint d6ent, uint actorIndex, ref Transform pose);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setLinearLimit(IntPtr regPtr, uint d6ent, D6Axis axis, ref JointLinearLimitPair limit);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setTwistLimit(IntPtr regPtr, uint d6ent, ref JointAngularLimitPair limit);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setPyramidSwingLimit(IntPtr regPtr, uint d6ent, ref JointLimitPyramid limit);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setSwingLimit(IntPtr regPtr, uint d6ent, ref JointLimitCone limit);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setDrive(IntPtr regPtr, uint d6ent, D6Drive drive, ref D6JointDrive jointDrive);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setBreakForce(IntPtr regPtr, uint d6ent, float breakForce);

        [DllImport(Engine.NativeModule)]
        private static extern float d6joint_getBreakForce(IntPtr regPtr, uint d6ent);

        [DllImport(Engine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool d6joint_isBroken(IntPtr regPtr, uint d6ent);

        [DllImport(Engine.NativeModule)]
        private static extern uint d6joint_getAttached(IntPtr regPtr, uint d6ent);

        [DllImport(Engine.NativeModule)]
        private static extern void d6joint_setAttached(IntPtr regPtr, uint d6ent, uint attachedEnt);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("D6 Joint");

                return cachedMetadata!;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        public Entity Target
        {
            get => new Entity(d6joint_getTarget(regPtr, entityId));
            set => d6joint_setTarget(regPtr, entityId, value.ID);
        }

        public Entity Attached
        {
            get => new Entity(d6joint_getAttached(regPtr, entityId));
            set => d6joint_setAttached(regPtr, entityId, value.ID);
        }

        public Transform LocalPose
        {
            get
            {
                Transform p = new Transform();
                d6joint_getLocalPose(regPtr, entityId, 0, out p);
                return p;
            }
            set
            {
                d6joint_setLocalPose(regPtr, entityId, 0, ref value);
            }
        }

        public Transform TargetLocalPose
        {
            get
            {
                Transform p = new Transform();
                d6joint_getLocalPose(regPtr, entityId, 1, out p);
                return p;
            }
            set
            {
                d6joint_setLocalPose(regPtr, entityId, 1, ref value);
            }
        }

        public float BreakForce
        {
            get => d6joint_getBreakForce(regPtr, entityId);
            set => d6joint_setBreakForce(regPtr, entityId, value);
        }

        public bool IsBroken => d6joint_isBroken(regPtr, entityId);

        internal D6Joint(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }

        public void SetAxisMotion(D6Axis axis, D6Motion motion)
        {
            d6joint_setAxisMotion(regPtr, entityId, axis, motion);
        }

        public void SetAllLinearAxisMotion(D6Motion motion)
        {
            SetAxisMotion(D6Axis.X, motion);
            SetAxisMotion(D6Axis.Y, motion);
            SetAxisMotion(D6Axis.Z, motion);
        }

        public void SetAllAngularAxisMotion(D6Motion motion)
        {
            SetAxisMotion(D6Axis.AngularX, motion);
            SetAxisMotion(D6Axis.AngularY, motion);
            SetAxisMotion(D6Axis.AngularZ, motion);
        }

        public void SetAllAxisMotion(D6Motion motion)
        {
            SetAllLinearAxisMotion(motion);
            SetAllAngularAxisMotion(motion);
        }

        public void SetLinearLimit(D6Axis axis, JointLinearLimitPair limit)
        {
            d6joint_setLinearLimit(regPtr, entityId, axis, ref limit);
        }

        public void SetTwistLimit(JointAngularLimitPair limit)
        {
            d6joint_setTwistLimit(regPtr, entityId, ref limit);
        }

        public void SetPyramidSwingLimit(JointLimitPyramid limit)
        {
            d6joint_setPyramidSwingLimit(regPtr, entityId, ref limit);
        }

        public void SetSwingLimit(JointLimitCone limit)
        {
            d6joint_setSwingLimit(regPtr, entityId, ref limit);
        }

        public void SetDrive(D6Drive axis, D6JointDrive drive)
        {
            d6joint_setDrive(regPtr, entityId, axis, ref drive);
        }

        public void SetAllLinearDrives(D6JointDrive drive)
        {
            d6joint_setDrive(regPtr, entityId, D6Drive.X, ref drive);
            d6joint_setDrive(regPtr, entityId, D6Drive.Y, ref drive);
            d6joint_setDrive(regPtr, entityId, D6Drive.Z, ref drive);
        }
    }
}
