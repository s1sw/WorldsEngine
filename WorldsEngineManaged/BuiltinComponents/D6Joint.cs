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

    public class D6Joint : BuiltinComponent
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void d6joint_setTarget(IntPtr regPtr, uint d6ent, uint target);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint d6joint_getTarget(IntPtr regPtr, uint d6ent);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void d6joint_setAxisMotion(IntPtr regPtr, uint d6ent, D6Axis axis, D6Motion motion);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void d6joint_getLocalPose(IntPtr regPtr, uint d6ent, uint actorIndex, out Transform pose);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void d6joint_setLocalPose(IntPtr regPtr, uint d6ent, uint actorIndex, ref Transform pose);

        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("D6 Joint");

                return cachedMetadata;
            }
        }

        private static ComponentMetadata cachedMetadata;

        public Entity Target
        {
            get
            {
                return new Entity(d6joint_getTarget(regPtr, entityId));
            }

            set
            {
                d6joint_setTarget(regPtr, entityId, value.ID);
            }
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
    }
}
