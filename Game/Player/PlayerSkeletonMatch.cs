using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Audio;
using WorldsEngine.Util;
using Game.Util;

namespace Game.Player;

[Component]
public class PlayerSkeletonMatch : Component, IStartListener, IUpdateableComponent
{
    private Transform _initialT;
    private Transform _lhToWorld = new(Vector3.Zero, Quaternion.Identity);
    private Transform _rhToWorld = new(Vector3.Zero, Quaternion.Identity);
    private TwoBoneIK _leftHandIK;
    private TwoBoneIK _rightHandIK;
    private TwoBoneIK _legIK;

    private static Vector3 groundOffset = new(0.0f, -0.95f, -0.23f);

    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        _initialT = swo.GetBoneTransform(root);

        var mesh = MeshManager.GetMesh(swo.Mesh);
        var lh = mesh.GetBone("hand_L");
        var rh = mesh.GetBone("hand_R");

        int parentIdx = (int)lh.Parent;

        while (parentIdx != -1)
        {
            Bone b = mesh.GetBone(parentIdx);
            _lhToWorld = _lhToWorld.TransformBy(b.RestPose);
            parentIdx = mesh.GetBone(parentIdx).Parent;
        }

        parentIdx = rh.Parent;

        while (parentIdx != -1)
        {
            Bone b = mesh.GetBone(parentIdx);
            _rhToWorld = _rhToWorld.TransformBy(b.RestPose);
            parentIdx = mesh.GetBone(parentIdx).Parent;
        }

        var lowerArm = mesh.GetBone("lowerarm_L");
        _leftHandIK = new TwoBoneIK(lowerArm.RestPose.Position.Length, mesh.GetBone("hand_L").RestPose.Position.Length, lowerArm.RestPose.Position);

        var lowerArmR = mesh.GetBone("lowerarm_R");
        _rightHandIK = new TwoBoneIK(lowerArm.RestPose.Position.Length, mesh.GetBone("hand_R").RestPose.Position.Length, lowerArm.RestPose.Position);

        var lowerLegR = mesh.GetBone("calf_R");
        _legIK = new TwoBoneIK(lowerLegR.RestPose.Position.Length, mesh.GetBone("foot_R").RestPose.Position.Length, lowerLegR.RestPose.Position);
    }

    public void Update()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        var rootTransform = new Transform(groundOffset, _initialT.Rotation);
        swo.SetBoneTransform(MeshManager.GetBoneIndex(swo.Mesh, "root"), rootTransform);

        LeftArmUpdate();
        RightArmUpdate();

        UpdateFeetTargets();
        LeftLegUpdate();
        RightLegUpdate();
    }

    private void LeftArmUpdate()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var wsTarget = LocalPlayerSystem.LeftHand.Transform;
        var targetTransform = wsTarget.TransformByInverse(Entity.Transform);
        // Fix rotation
        targetTransform.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, -MathF.PI * 0.5f));
        targetTransform = targetTransform.TransformBy(Entity.Transform);

        var mesh = MeshManager.GetMesh(swo.Mesh);
        var lowerArm = mesh.GetBone("lowerarm_L");
        var upperArm = mesh.GetBone("upperarm_L");
        var hand = mesh.GetBone("hand_L");

        var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);

        targetTransform.Position = upperArmWS.Position + (targetTransform.Position - upperArmWS.Position).ClampMagnitude(_leftHandIK.LowerLength + _leftHandIK.UpperLength);
        swo.SetBoneWorldSpaceTransform(hand.ID, targetTransform);

        // Calculate pole
        Vector3 poleCandidate1 = wsTarget.TransformDirection(Vector3.Right);
        Vector3 poleCandidate2 = Entity.Transform.TransformDirection(Vector3.Right);
        float blendFac = MathF.Pow(1f - MathFX.Saturate(poleCandidate1.Dot(Vector3.Up)), 2.0f);
        Vector3 perpendicularPole = Vector3.Lerp(poleCandidate1, poleCandidate2, blendFac);
        Vector3 pole = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), perpendicularPole);

        var upperRotation = _leftHandIK.GetUpperRotation(upperArmWS, wsTarget, pole);
        upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));

        swo.SetBoneWorldSpaceTransform(upperArm.ID, upperArmWS);

        var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
        var lowerRotation = _leftHandIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
        lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));
        swo.SetBoneWorldSpaceTransform(lowerArm.ID, lowerArmWS);
    }

    private void RightArmUpdate()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var wsTarget = LocalPlayerSystem.RightHand.Transform;
        var targetTransform = wsTarget.TransformByInverse(Entity.Transform);
        // Fix rotation
        targetTransform.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));
        targetTransform = targetTransform.TransformBy(Entity.Transform);

        var mesh = MeshManager.GetMesh(swo.Mesh);
        var lowerArm = mesh.GetBone("lowerarm_R");
        var upperArm = mesh.GetBone("upperarm_R");
        var hand = mesh.GetBone("hand_R");

        var upperArmWS = swo.GetBoneWorldSpaceTransform(upperArm.ID);

        targetTransform.Position = upperArmWS.Position + (targetTransform.Position - upperArmWS.Position).ClampMagnitude(_rightHandIK.LowerLength + _rightHandIK.UpperLength);
        swo.SetBoneWorldSpaceTransform(hand.ID, targetTransform);


        // Calculate pole
        Vector3 poleCandidate1 = wsTarget.TransformDirection(Vector3.Left);
        Vector3 poleCandidate2 = Entity.Transform.TransformDirection(Vector3.Left);
        float blendFac = MathF.Pow(1f - MathFX.Saturate(poleCandidate1.Dot(Vector3.Up)), 2.0f);
        Vector3 perpendicularPole = Vector3.Lerp(poleCandidate1, poleCandidate2, blendFac);
        Vector3 pole = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), perpendicularPole);

        var upperRotation = _leftHandIK.GetUpperRotation(upperArmWS, wsTarget, pole);
        upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));

        swo.SetBoneWorldSpaceTransform(upperArm.ID, upperArmWS);

        var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
        var lowerRotation = _leftHandIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
        lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));
        swo.SetBoneWorldSpaceTransform(lowerArm.ID, lowerArmWS);
    }

    private Vector3 _leftFootTarget = new();
    private Vector3 _rightFootTarget = new();

    private Vector3 _nextLeftStep;
    private Vector3 _nextRightStep;
    private float _leftStepProgress = 0.0f;
    private float _rightStepProgress = 0.0f;

    private Vector3 _oldLpos;
    private Vector3 _oldRpos;

    private bool _leftStep = false;
    private bool _rightStep = false;
    private float _timeSinceMovement = 0.0f;
    private float _timeSinceMovementStart = 0.0f;
    private bool _standingStraight = false;
    private bool _wasLastStepLeft = false;
    private Vector3 _lastGroundPos;

    private void UpdateFeetTargets()
    {
        const float footX = 0.19f;
        const float stepTime = 0.15f;

        Vector3 localLF = groundOffset + Vector3.Left * footX;
        Vector3 localRF = groundOffset + Vector3.Right * footX;
        Vector3 velocity = Entity.GetComponent<DynamicPhysicsActor>().Velocity;
        float stride = _legIK.LowerLength + _legIK.UpperLength * velocity.Length / 3.0f;
        bool grounded = Entity.GetComponent<PlayerRig>().Grounded;

        if (!grounded)
        {
            _leftFootTarget = Entity.Transform.TransformPoint(localLF) + Vector3.Up * 0.2f;
            _rightFootTarget = Entity.Transform.TransformPoint(localRF) + Vector3.Up * 0.2f;
            return;
        }

        if (velocity.Length < 0.1f)
        {
            _timeSinceMovement += Time.DeltaTime;
            _timeSinceMovementStart = 0.0f;
        }
        else
        {
            _timeSinceMovement = 0.0f;
            _timeSinceMovementStart += Time.DeltaTime;
        }

        Vector3 movementDir = Entity.Transform.InverseTransformDirection(velocity.Normalized);

        if (_timeSinceMovement > 0f && !_standingStraight)
        {
            _leftStepProgress = 0.0f;
            _oldLpos = _leftFootTarget;
            _nextLeftStep = Entity.Transform.TransformPoint(localLF);
            _leftStep = true;

            _rightStepProgress = 0.0f;
            _oldRpos = _rightFootTarget;
            _nextRightStep = Entity.Transform.TransformPoint(localRF);
            _rightStep = true;

            _standingStraight = true;
        }
        else if (_timeSinceMovementStart > 0f && _standingStraight)
        {
            _standingStraight = false;

            _leftStepProgress = 0.0f;
            _oldLpos = _leftFootTarget;
            _nextLeftStep = Entity.Transform.TransformPoint(localLF + movementDir * stride);
            _leftStep = true;
            _wasLastStepLeft = true;
        }

        if (_standingStraight)
        {
            float leftDist = Entity.Transform.TransformPoint(localLF).DistanceTo(_leftFootTarget);
            float rightDist = Entity.Transform.TransformPoint(localRF).DistanceTo(_rightFootTarget);

            if (rightDist > 0.3f && !_rightStep)
            {
                _rightStepProgress = 0.0f;
                _oldRpos = _rightFootTarget;
                _nextRightStep = Entity.Transform.TransformPoint(localRF);
                _rightStep = true;
            }

            if (leftDist > 0.3f && !_leftStep)
            {
                _leftStepProgress = 0.0f;
                _oldLpos = _leftFootTarget;
                _nextLeftStep = Entity.Transform.TransformPoint(localLF);
                _leftStep = true;
            }
        }

        if (Entity.Transform.TransformPoint(groundOffset).DistanceTo(_lastGroundPos) > stride)
        {
            if (_wasLastStepLeft)
            {
                _rightStepProgress = 0.0f;
                _oldRpos = _nextRightStep;
                _nextRightStep = Entity.Transform.TransformPoint(localRF + movementDir * stride);
                _rightStep = true;
                _wasLastStepLeft = false;
            }
            else
            {
                _wasLastStepLeft = false;
                _leftStepProgress = 0.0f;
                _oldLpos = _nextLeftStep;
                _nextLeftStep = Entity.Transform.TransformPoint(localLF + movementDir * stride);
                _leftStep = true;
                _wasLastStepLeft = true;
            }
            _lastGroundPos = Entity.Transform.TransformPoint(groundOffset);
        }

        if (_leftStep)
        {
            _leftStepProgress += Time.DeltaTime / stepTime;
            _leftFootTarget = Vector3.Lerp(_oldLpos, _nextLeftStep, _leftStepProgress);
            _leftFootTarget.y += MathF.Sin(_leftStepProgress * MathF.PI) * 0.25f;

            if (_leftStepProgress >= 1f)
            {
                _leftStep = false;
                Audio.PlayOneShotEvent("event:/Player/Walking", _leftFootTarget);
            }
        }


        if (_rightStep)
        {
            _rightStepProgress += Time.DeltaTime / stepTime;
            _rightFootTarget = Vector3.Lerp(_oldRpos, _nextRightStep, _rightStepProgress);
            _rightFootTarget.y += MathF.Sin(_rightStepProgress * MathF.PI) * 0.25f;

            if (_rightStepProgress >= 1f)
            {
                _rightStep = false;
                Audio.PlayOneShotEvent("event:/Player/Walking", _rightFootTarget);
            }
        }
    }

    private void RightLegUpdate()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var mesh = MeshManager.GetMesh(swo.Mesh);
        var calf = mesh.GetBone("calf_R");
        var thigh = mesh.GetBone("thigh_R");
        var foot = mesh.GetBone("foot_R");

        var wsTarget = new Transform(_rightFootTarget, Quaternion.Identity);

        var upperArmWS = swo.GetBoneComponentSpaceTransform(thigh.ID).TransformBy(Entity.Transform);

        // Calculate pole
        Vector3 pole = Entity.Transform.TransformDirection(Vector3.Right);

        var upperRotation = _legIK.GetUpperRotation(upperArmWS, wsTarget, -pole);
        //upperRotation = upperRotation * Quaternion.AngleAxis(MathF.PI, Vector3.Forward);
        upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, -MathF.PI * 0.5f));

        swo.SetBoneWorldSpaceTransform(thigh.ID, upperArmWS);

        var lowerArmWS = swo.GetBoneComponentSpaceTransform(calf.ID).TransformBy(Entity.Transform);
        var lowerRotation = _legIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
        //lowerRotation = lowerRotation * Quaternion.AngleAxis(MathF.PI, Vector3.Forward);
        lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));
        swo.SetBoneWorldSpaceTransform(calf.ID, lowerArmWS);

        var footws = swo.GetBoneComponentSpaceTransform(foot.ID).TransformBy(Entity.Transform);
        footws.Rotation = Entity.Transform.Rotation * new Quaternion(new Vector3(-MathF.PI * 0.5f, MathF.PI, 0f));
        //swo.SetBoneWorldSpaceTransform(foot.ID, footws);
    }

    private void LeftLegUpdate()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var mesh = MeshManager.GetMesh(swo.Mesh);
        var calf = mesh.GetBone("calf_L");
        var thigh = mesh.GetBone("thigh_L");
        var foot = mesh.GetBone("foot_L");

        var wsTarget = new Transform(_leftFootTarget, Quaternion.Identity);

        var upperArmWS = swo.GetBoneComponentSpaceTransform(thigh.ID).TransformBy(Entity.Transform);

        // Calculate pole
        Vector3 pole = Entity.Transform.TransformDirection(Vector3.Left);

        var upperRotation = _legIK.GetUpperRotation(upperArmWS, wsTarget, pole);
        upperRotation = upperRotation * Quaternion.AngleAxis(MathF.PI, Vector3.Forward);
        upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0.0f, MathF.PI * 0.5f));

        swo.SetBoneWorldSpaceTransform(thigh.ID, upperArmWS);

        var lowerArmWS = swo.GetBoneComponentSpaceTransform(calf.ID).TransformBy(Entity.Transform);
        var lowerRotation = _legIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
        lowerRotation = lowerRotation * Quaternion.AngleAxis(MathF.PI, Vector3.Forward);
        upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0.0f, MathF.PI * 0.5f));
        lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.5f, 0f, MathF.PI * 0.5f));
        swo.SetBoneWorldSpaceTransform(calf.ID, lowerArmWS);

        var footws = swo.GetBoneComponentSpaceTransform(foot.ID).TransformBy(Entity.Transform);
        footws.Rotation = Entity.Transform.Rotation * new Quaternion(new Vector3(-MathF.PI * 0.5f, MathF.PI, 0f));
        //swo.SetBoneWorldSpaceTransform(foot.ID, footws);
    }
}