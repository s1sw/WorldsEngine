using ImGuiNET;
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
    private static Entity LeftHand;
    private static Entity RightHand;

    static Vector3 _nonVROffset = new Vector3(0.125f, -0.2f, 0.55f);
    const float TorqueLimit = 35f;
    float RotationDMax = 15.0f;
    float RotationPMax = 600.0f;

    float PositionP = 2003f;
    float PositionD = 100f;

    public bool FollowRightHand = false;

    [NonSerialized]
    public bool UseOverrideTensor = false;
    [NonSerialized]
    public Mat3x3 OverrideTensor = new();
    public float TorqueFactor = 1.0f;

    [NonSerialized]
    public Transform? OverrideTarget = null;

    private Transform _targetTransform = new Transform();

    private Transform _offsetTransform = new(Vector3.Zero, Quaternion.Identity);

    [EditableClass]
    public QuaternionEuroFilter RotationFilter = new();
    public bool DisableForces = false;

#if DEBUG_HAND_VIS
        private Entity _visEntity;
#endif

    private bool _moveToCenter = false;

    public void Start()
    {
        //if (FollowRightHand)
        //    rotationOffset = new Quaternion(0.348f, 0.254f, -0.456f, -0.779f);
        //else
        //    rotationOffset = new Quaternion(-0.409f, -0.154f, 0.419f, 0.796f);


#if DEBUG_HAND_VIS
            _visEntity = Registry.Create();
            var currentWO = Registry.GetComponent<SkinnedWorldObject>(Entity);

            var duplWo = Registry.AddComponent<WorldObject>(_visEntity);
            duplWo.Mesh = currentWO.Mesh;
            Registry.SetName(_visEntity, $"{Registry.GetName(Entity)} visualiser");
#endif
        if (FollowRightHand)
            RightHand = Entity;
        else
            LeftHand = Entity;
    }

    public void Think()
    {
        SetTargets();

#if DEBUG_HAND_VIS
        _targetTransform.Scale = Vector3.One;
        Registry.SetTransform(_visEntity, _targetTransform);
#endif

        UpdatePosition();
        AvoidSpin();
        UpdateRotation();
    }

    private void UpdatePosition()
    {
        var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(LocalPlayerSystem.PlayerBody);
        var dpa = Entity.GetComponent<DynamicPhysicsActor>();

        float forceScale = 1f;
        HandGrab otherGrab = FollowRightHand ? LeftHand.GetComponent<HandGrab>() : RightHand.GetComponent<HandGrab>();
        HandGrab thisGrab = Entity.GetComponent<HandGrab>();

        if (thisGrab.GrippedEntity.IsValid && otherGrab.GrippedEntity == thisGrab.GrippedEntity)
            forceScale *= 0.7f;

        Vector3 pForce = (_targetTransform.Position - dpa.Pose.Position) * PositionP;
        Vector3 dForce = (dpa.Velocity - bodyDpa.Velocity) * PositionD;

        var hg = Entity.GetComponent<HandGrab>();

        Vector3 force = (pForce - dForce) * forceScale;

        if (!DisableForces)
        {
            if (hg.GrippedEntity.IsValid && hg.GrippedEntity.TryGetComponent<DynamicPhysicsActor>(out DynamicPhysicsActor gripped))
            {
                dpa.AddForce(force);
            }
            else
            {
                dpa.AddForce(force);
            }
            bodyDpa.AddForce(-force);

        }
    }

    private void AvoidSpin()
    {
        var dpa = Entity.GetComponent<DynamicPhysicsActor>();
        // Desparate Spin Avoidance
        if (dpa.AngularVelocity.Length > 500.0f)
        {
            dpa.MaxAngularVelocity = 10.0f;
            var hg = Entity.GetComponent<HandGrab>();
            if (Registry.Valid(hg.GrippedEntity) && hg.GrippedEntity.HasComponent<DynamicPhysicsActor>())
            {
                var grabbedDpa = hg.GrippedEntity.GetComponent<DynamicPhysicsActor>();
                grabbedDpa.AngularVelocity = grabbedDpa.AngularVelocity.ClampMagnitude(15.0f);
            }
        }
        else
        {
            dpa.MaxAngularVelocity = 1000.0f;
        }
    }

    private void UpdateRotation()
    {
        var dpa = Entity.GetComponent<DynamicPhysicsActor>();
        var pose = dpa.Pose;
        Quaternion relativeRotation = pose.Rotation;
        Quaternion targetRotation = _targetTransform.Rotation;//RotationFilter.Filter(_targetTransform.Rotation, Time.DeltaTime);
        Quaternion quatDiff = targetRotation * relativeRotation.Inverse;
        quatDiff = quatDiff.Normalized;

        float angle = PDUtil.AngleToErr(quatDiff.Angle);
        Vector3 axis = quatDiff.Axis;

        Vector3 pTorque = angle * axis * RotationPMax;
        Vector3 dTorque = dpa.AngularVelocity * RotationDMax;

        Vector3 torque = (pTorque - dTorque);

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

        torque = pose.Rotation.SingleCover * torque * TorqueFactor;

        //if (torque.LengthSquared > 0.5f)
        //    torque = torque.ClampMagnitude(TorqueLimit);
        
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

            _targetTransform.Position = camT.TransformPoint(offset);
            return;
        }

        Transform handT = FollowRightHand ? VRTransforms.RightHandTransform : VRTransforms.LeftHandTransform;
        //_targetTransform = _offsetTransform.TransformBy(_targetTransform);
        BoneTransforms bt = FollowRightHand ? VR.RightHandBones : VR.LeftHandBones;
        Transform conversion = new(Vector3.Zero, Quaternion.AngleAxis(MathF.PI, Vector3.Up));
        //_targetTransform = handT.TransformBy(conversion).TransformBy(bt[0]).TransformBy(bt[1]);
        _targetTransform = bt[1].TransformBy(bt[0]).TransformBy(conversion).TransformBy(handT);

        if (_targetTransform.Rotation.HasNaNComponent)
            _targetTransform.Rotation = Quaternion.Identity;

        if (_targetTransform.Position.HasNaNComponent)
            _targetTransform.Position = Vector3.Zero;

        Quaternion virtualRotation = Camera.Main.Rotation;
        _targetTransform.Position = virtualRotation * _targetTransform.Position;

        _targetTransform.Position += PlayerCameraSystem.GetCamPosForSimulation();
        _targetTransform.Rotation = virtualRotation * _targetTransform.Rotation;
    }

    public void Update()
    {
        if (Keyboard.KeyPressed(KeyCode.T))
            _moveToCenter = !_moveToCenter;

        ImGui.DragFloat("PosP", ref PositionP);
        ImGui.DragFloat("PosD", ref PositionD);
        ImGui.DragFloat("RotP", ref RotationPMax);
        ImGui.DragFloat("RotD", ref RotationDMax);
    }
}
