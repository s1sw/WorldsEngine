using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    [EditorFriendlyName("Physics Hand")]
    class PhysHand : IThinkingComponent, IStartListener
    {
        static Vector3 _nonVROffset = new Vector3(0.1f, -0.2f, 0.55f);
        const float ForceLimit = 1000f;
        const float TorqueLimit = 15f;

        private Quaternion rotationOffset = Quaternion.Identity;

        [EditableClass]
        public StablePD PD = new StablePD();
        [EditableClass]
        public V3PidController RotationPID = new V3PidController();
        public bool FollowRightHand = false;

        [NonSerialized]
        public bool UseOverrideTensor = false;
        [NonSerialized]
        public Mat3x3 OverrideTensor = new();

        [NonSerialized]
        public Transform? OverrideTarget = null;

        private Transform _targetTransform = new Transform();

#if DEBUG_HAND_VIS
        private Entity _visEntity;
#endif

        public void Start(Entity entity)
        {
            if (FollowRightHand)
                rotationOffset = new Quaternion(0.348f, 0.254f, -0.456f, -0.779f);
            else
                rotationOffset = new Quaternion(-0.409f, -0.154f, 0.419f, 0.796f);

#if DEBUG_HAND_VIS
            _visEntity = Registry.Create();
            var currentWO = Registry.GetComponent<WorldObject>(entity);

            var duplWo = Registry.AddComponent<WorldObject>(_visEntity);
            duplWo.Mesh = currentWO.Mesh;
            Registry.SetName(_visEntity, $"{Registry.GetName(entity)} visualiser");
#endif
        }

        public void Think(Entity entity)
        {
            SetTargets();
            var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(PlayerRigSystem.PlayerBody);

#if DEBUG_HAND_VIS
            _targetTransform.Scale = Vector3.One;
            Registry.SetTransform(_visEntity, _targetTransform);
#endif

            var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = dpa.Pose;

            Vector3 force = PD.CalculateForce(pose.Position, _targetTransform.Position + (bodyDpa.Velocity * Time.DeltaTime), dpa.Velocity, Time.DeltaTime, bodyDpa.Velocity)
                .ClampMagnitude(ForceLimit);

            dpa.AddForce(force);
            bodyDpa.AddForce(-force);

            Quaternion quatDiff = _targetTransform.Rotation.SingleCover * pose.Rotation.SingleCover.Inverse;
            quatDiff = quatDiff.SingleCover;

            float angle = PDUtil.AngleToErr(quatDiff.Angle);
            Vector3 axis = quatDiff.Axis;

            Vector3 torque = RotationPID.CalculateForce(angle * axis, Time.DeltaTime);

            torque = pose.Rotation.SingleCover.Inverse * torque;
            if (!UseOverrideTensor)
            {
                Mat3x3 inertiaTensor = new(
                    new Vector3(dpa.InertiaTensor.x, 0.0f, 0.0f),
                    new Vector3(0.0f, dpa.InertiaTensor.y, 0.0f),
                    new Vector3(0.0f, 0.0f, dpa.InertiaTensor.z));
                Quaternion itRotation = dpa.CenterOfMassLocalPose.Rotation.SingleCover;
                Mat3x3 finalTensor = (Mat3x3)itRotation * inertiaTensor * (Mat3x3)itRotation.Inverse;

                torque = finalTensor.Transform(torque);
            }
            else
            {
                torque = OverrideTensor.Transform(torque);
            }

            torque = pose.Rotation.SingleCover * torque;

            //torque = torque.ClampMagnitude(TorqueLimit);
            dpa.AddTorque(torque);
        }
        private void SetTargets()
        {
            if (OverrideTarget != null)
            {
                _targetTransform = OverrideTarget.Value;
                return;
            }

            if (!VR.Enabled)
            {
                _targetTransform.Rotation = Camera.Main.Rotation;
                Vector3 offset = _nonVROffset;

                if (FollowRightHand)
                    offset.x *= -1.0f;

                Transform camT = new(PlayerCameraSystem.GetCamPosForSimulation(), Camera.Main.Rotation);

                _targetTransform.Position = camT.TransformPoint(offset);
                return;
            }

            _targetTransform = FollowRightHand ? VR.RightHandTransform : VR.LeftHandTransform;

            if (_targetTransform.Rotation.HasNaNComponent)
                _targetTransform.Rotation = Quaternion.Identity;

            if (_targetTransform.Position.HasNaNComponent)
                _targetTransform.Position = Vector3.Zero;

            Quaternion virtualRotation = Camera.Main.Rotation;
            _targetTransform.Position = virtualRotation * _targetTransform.Position;

            _targetTransform.Position += PlayerCameraSystem.GetCamPosForSimulation();
            _targetTransform.Rotation *= rotationOffset;
            _targetTransform.Rotation = virtualRotation * _targetTransform.Rotation;
        }
    }
}
