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
    class DummyHand : Component, IThinkingComponent
    {
        static Vector3 _nonVROffset = new Vector3(0.1f, -0.2f, 0.55f);

        public bool FollowRightHand = false;

        private Quaternion rotationOffset = Quaternion.Identity;

        private Transform _targetTransform = new Transform();
        private void SetTargets()
        {
            if (!VR.Enabled)
            {
                _targetTransform.Rotation = Camera.Main.Rotation;
                Vector3 offset = _nonVROffset;

                if (FollowRightHand)
                    offset.x *= -1.0f;

                Transform camT = new Transform(Camera.Main.Position, Camera.Main.Rotation);

                camT.Position = PlayerCameraSystem.GetCamPosForSimulation();

                _targetTransform.Position = camT.TransformPoint(offset);
                return;
            }

            _targetTransform = FollowRightHand ? VRTransforms.RightHandTransform : VRTransforms.LeftHandTransform;

            if (_targetTransform.Rotation.HasNaNComponent)
                _targetTransform.Rotation = Quaternion.Identity;

            if (_targetTransform.Position.HasNaNComponent)
                _targetTransform.Position = Vector3.Zero;

            Quaternion virtualRotation = Camera.Main.Rotation;//* Quaternion.AngleAxis(MathF.PI, Vector3.Up);
            //
            //_targetTransform.Position += _targetTransform.Rotation * PositionOffset;
            //_targetTransform.Position = virtualRotation * _targetTransform.Position;
            //

            _targetTransform.Position += PlayerCameraSystem.GetCamPosForSimulation();//Camera.Main.Position;
            //
            _targetTransform.Rotation *= rotationOffset;
            //_targetTransform.Rotation *= new Quaternion(EulerRotationOffset);
            //
            _targetTransform.Rotation = virtualRotation * _targetTransform.Rotation;
        }

        public void Think()
        {
            SetTargets();
            Transform goTo = new Transform(_targetTransform.Position, _targetTransform.Rotation);

            Registry.SetTransform(Entity, goTo);
        }

        public void Start()
        {
            if (FollowRightHand)
                rotationOffset = new Quaternion(0.348f, 0.254f, -0.456f, -0.779f);
            else
                rotationOffset = new Quaternion(-0.409f, -0.154f, 0.419f, 0.796f);
        }
    }
}
