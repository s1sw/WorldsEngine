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
    class PhysHand : IThinkingComponent
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

        private Transform _targetTransform = new Transform();

        private void SetTargets()
        {
            if (!VR.Enabled)
            {
                _targetTransform.Rotation = Camera.Main.Rotation;
                Vector3 offset = _nonVROffset;

                if (FollowRightHand)
                    offset.x *= -1.0f;

                _targetTransform.Position = Camera.Main.TransformPoint(offset);
                return;
            }

            _targetTransform = FollowRightHand ? VR.RightHandTransform : VR.LeftHandTransform;

            if (_targetTransform.Rotation.HasNaNComponent)
                _targetTransform.Rotation = Quaternion.Identity;
            
            if (_targetTransform.Position.HasNaNComponent)
                _targetTransform.Position = Vector3.Zero;

            Quaternion virtualRotation = Camera.Main.Rotation;//* Quaternion.AngleAxis(MathF.PI, Vector3.Up);
            //
            //_targetTransform.Position += _targetTransform.Rotation * PositionOffset;
            //_targetTransform.Position = virtualRotation * _targetTransform.Position;
            //
            
            _targetTransform.Position += Camera.Main.Position;
            //
            _targetTransform.Rotation *= rotationOffset;
            //_targetTransform.Rotation *= new Quaternion(EulerRotationOffset);
            //
            _targetTransform.Rotation = virtualRotation * _targetTransform.Rotation;
        }

        public void Think(Entity entity)
        {
            SetTargets();

            var dpa = Registry.GetComponent<DynamicPhysicsActor>(entity);
            Transform pose = dpa.Pose;

            Vector3 force = PD.CalculateForce(pose.Position, _targetTransform.Position, dpa.Velocity, Time.DeltaTime)
                .ClampMagnitude(ForceLimit);

            dpa.AddForce(force);

            Quaternion quatDiff = _targetTransform.Rotation.SingleCover * pose.Rotation.SingleCover.Inverse;
            quatDiff = quatDiff.SingleCover;
            
            float angle = PDUtil.AngleToErr(quatDiff.Angle);
            Vector3 axis = quatDiff.Axis;
            
            Vector3 torque = RotationPID.CalculateForce(angle * axis, Time.DeltaTime);

            Quaternion itRotation = dpa.CenterOfMassLocalPose.Rotation;
            Vector3 tensor = dpa.InertiaTensor;

            torque = itRotation * torque;
            torque *= tensor;

            torque = itRotation.Inverse * torque;

            torque = torque.ClampMagnitude(TorqueLimit);
            dpa.AddTorque(torque);
            
            if (Keyboard.KeyPressed(KeyCode.L) && !FollowRightHand)
            {
                rotationOffset = VR.LeftHandTransform.Rotation.Inverse;
                Logger.Log($"New rotation offset for left hand: {rotationOffset}");
            }

            if (Keyboard.KeyPressed(KeyCode.R) && FollowRightHand)
            {
                rotationOffset = VR.RightHandTransform.Rotation.Inverse;
                Logger.Log($"New rotation offset for right hand: {rotationOffset}");
            }
        }
    }
}
