using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Game.Interaction;
using WorldsEngine;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game.Player;

[Component]
[EditorFriendlyName("Physics Hand")]
class PhysHand : Component, IThinkingComponent, IStartListener, IUpdateableComponent
{
    static Vector3 _nonVROffset = new Vector3(0.125f, -0.2f, 0.55f);
    const float TorqueLimit = 15f;

    private Quaternion rotationOffset = Quaternion.Identity;

    [EditableClass]
    public V3PidController PD = new();

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
    [EditableClass]
    public QuaternionEuroFilter RotationFilter = new();
    public bool DisableForces = false;

#if DEBUG_HAND_VIS
        private Entity _visEntity;
#endif

    private bool _moveToCenter = false;

    public void Start()
    {
        if (FollowRightHand)
            rotationOffset = new Quaternion(0.348f, 0.254f, -0.456f, -0.779f);
        else
            rotationOffset = new Quaternion(-0.409f, -0.154f, 0.419f, 0.796f);

#if DEBUG_HAND_VIS
            _visEntity = Registry.Create();
            var currentWO = Registry.GetComponent<SkinnedWorldObject>(Entity);

            var duplWo = Registry.AddComponent<WorldObject>(_visEntity);
            duplWo.Mesh = currentWO.Mesh;
            Registry.SetName(_visEntity, $"{Registry.GetName(Entity)} visualiser");
#endif
    }

    private Vector3 lastAppliedVel = Vector3.Zero;

    public void Think()
    {
        var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(LocalPlayerSystem.PlayerBody);
        SetTargets();

        var dpa = Registry.GetComponent<DynamicPhysicsActor>(Entity);
        Transform pose = dpa.Pose;

        var rig = Registry.GetComponent<PlayerRig>(LocalPlayerSystem.PlayerBody);

        Vector3 velDiff = bodyDpa.Velocity;
        dpa.AddForce(velDiff - lastAppliedVel, ForceMode.VelocityChange);
        lastAppliedVel = velDiff;

#if DEBUG_HAND_VIS
        _targetTransform.Scale = Vector3.One;
        Registry.SetTransform(_visEntity, _targetTransform);
#endif

        //Vector3 force = PD.CalculateForce(pose.TransformByInverse(fakePose).Position + velDiff * Time.DeltaTime, _targetTransform.Position, dpa.Velocity - velDiff, Time.DeltaTime, Vector3.Zero);
        Vector3 force = PD.CalculateForce(_targetTransform.Position - pose.Position - velDiff * Time.DeltaTime, Time.DeltaTime, Vector3.Zero);

        if (!DisableForces)
        {
            dpa.AddForce(force);
            bodyDpa.AddForce(-force);
        }

        Quaternion relativeRotation = pose.Rotation;
        Quaternion targetRotation = _targetTransform.Rotation;//RotationFilter.Filter(_targetTransform.Rotation, Time.DeltaTime);
        Quaternion quatDiff = targetRotation * relativeRotation.Inverse;
        quatDiff = quatDiff.Normalized;

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

        if (torque.LengthSquared > 0.5f)
            torque = torque.ClampMagnitude(TorqueLimit);
        if (!DisableForces)
        {
            dpa.AddTorque(torque);
        }
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

            if (_moveToCenter)
            {
                offset.y = -0.15f;
                offset.z = 0.2f;
                if (FollowRightHand)
                {
                    offset.x = -0.05f;
                }
                else
                {
                    offset.x = 0.05f;
                }
                offset.x += 0.01f;
                _targetTransform.Rotation = Camera.Main.Rotation * Quaternion.AngleAxis(-0.08f, Vector3.Right);
            }

            Transform camT = new(PlayerCameraSystem.GetCamPosForSimulation(), Camera.Main.Rotation);

            Transform fakePose = LocalPlayerSystem.PlayerBody.GetComponent<DynamicPhysicsActor>().Pose;
            fakePose.Rotation = Quaternion.Identity;
            _targetTransform.Position = camT.TransformPoint(offset);
            //_targetTransform = _targetTransform.TransformByInverse(fakePose);
            return;
        }

        _targetTransform = FollowRightHand ? VRTransforms.RightHandTransform : VRTransforms.LeftHandTransform;

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

    public void Update()
    {
        if (Keyboard.KeyPressed(KeyCode.T))
            _moveToCenter = !_moveToCenter;
    }
}
