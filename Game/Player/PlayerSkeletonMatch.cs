using System;
using WorldsEngine;
using WorldsEngine.Math;
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

    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        _initialT = swo.GetBoneTransform(root);

        var mesh = MeshManager.GetMesh(swo.Mesh);
        var lh = mesh.GetBone("hand_L");
        var rh = mesh.GetBone("hand_R");
        Log.Msg($"lh id: {lh.ID}");
        Log.Msg($"lh name: {lh.Name}");
        Log.Msg($"rh id: {rh.ID}");
        Log.Msg($"rh name: {rh.Name}");

        int parentIdx = (int)lh.Parent;
        Log.Msg($"{lh.RestPose.Rotation}  {swo.GetBoneTransform((uint)lh.ID).Rotation}");

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
    }

    public void Update()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        var rootTransform = new Transform(Vector3.Down * 0.9f + Vector3.Backward * 0.23f, _initialT.Rotation);
        swo.SetBoneTransform(MeshManager.GetBoneIndex(swo.Mesh, "root"), rootTransform);
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");
        var rh = MeshManager.GetBoneIndex(swo.Mesh, "hand_R");

        {
            var wsTarget = LocalPlayerSystem.LeftHand.Transform;
            var targetTransform = wsTarget.TransformByInverse(Entity.Transform);
            // Fix rotation
            targetTransform.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, -MathF.PI * 0.25f));
            targetTransform = targetTransform.TransformBy(Entity.Transform);

            var mesh = MeshManager.GetMesh(swo.Mesh);
            var lowerArm = mesh.GetBone("lowerarm_L");
            var upperArm = mesh.GetBone("upperarm_L");
            var hand = mesh.GetBone("hand_L");

            swo.SetBoneWorldSpaceTransform(hand.ID, targetTransform, Entity.Transform);

            var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);

            // Calculate pole
            Vector3 poleCandidate1 = wsTarget.TransformDirection(Vector3.Right);
            Vector3 poleCandidate2 = Entity.Transform.TransformDirection(Vector3.Right);
            float blendFac = MathF.Pow(1f - MathFX.Saturate(poleCandidate1.Dot(Vector3.Up)), 2.0f);
            Vector3 perpendicularPole = Vector3.Lerp(poleCandidate1, poleCandidate2, blendFac);
            Vector3 pole = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), perpendicularPole);

            var upperRotation = _leftHandIK.GetUpperRotation(upperArmWS, wsTarget, pole);
            upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));

            swo.SetBoneWorldSpaceTransform(upperArm.ID, upperArmWS, Entity.Transform);

            var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
            var lowerRotation = _leftHandIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
            lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(lowerArm.ID, lowerArmWS, Entity.Transform);
        }

        {
            var wsTarget = LocalPlayerSystem.RightHand.Transform;
            var targetTransform = wsTarget.TransformByInverse(Entity.Transform);
            // Fix rotation
            targetTransform.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            targetTransform = targetTransform.TransformBy(Entity.Transform);

            var mesh = MeshManager.GetMesh(swo.Mesh);
            var lowerArm = mesh.GetBone("lowerarm_R");
            var upperArm = mesh.GetBone("upperarm_R");
            var hand = mesh.GetBone("hand_R");

            swo.SetBoneWorldSpaceTransform(hand.ID, targetTransform, Entity.Transform);

            var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);

            // Calculate pole
            Vector3 poleCandidate1 = wsTarget.TransformDirection(Vector3.Left);
            Vector3 poleCandidate2 = Entity.Transform.TransformDirection(Vector3.Left);
            float blendFac = MathF.Pow(1f - MathFX.Saturate(poleCandidate1.Dot(Vector3.Up)), 2.0f);
            Vector3 perpendicularPole = Vector3.Lerp(poleCandidate1, poleCandidate2, blendFac);
            Vector3 pole = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), perpendicularPole);

            var upperRotation = _leftHandIK.GetUpperRotation(upperArmWS, wsTarget, pole);
            upperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));

            swo.SetBoneWorldSpaceTransform(upperArm.ID, upperArmWS, Entity.Transform);

            var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
            var lowerRotation = _leftHandIK.GetLowerRotation(lowerArmWS, wsTarget, pole);
            lowerArmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(lowerArm.ID, lowerArmWS, Entity.Transform);
        }
    }
}