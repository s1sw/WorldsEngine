using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Util;

namespace Game.Player;

[Component]
public class PlayerSkeletonMatch : Component, IStartListener, IUpdateableComponent
{
    private Transform _initialT;
    private Transform _lhToWorld = new(Vector3.Zero, Quaternion.Identity);
    private Transform _rhToWorld = new(Vector3.Zero, Quaternion.Identity);

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
    }

    //  __b___
    //  \<v  /
    //   a  c
    //    \/ 
    //
    float CosineRule(float a, float b, float c)
    {
        float v = MathF.Acos(((a * a) + (b * b) - (c * c)) / (2 * a * b));

        return float.IsFinite(v) ? v : 0.0f;
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
            var lhb = mesh.GetBone("hand_L");
            var laT = swo.GetBoneTransform(lowerArm.ID);
            var hT = swo.GetBoneTransform(lhb.ID);

            swo.SetBoneWorldSpaceTransform(lhb.ID, targetTransform, Entity.Transform);

            float a = lowerArm.RestPose.Position.Length;
            float c = lhb.RestPose.Position.Length;

            var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);
            float b = upperArmWS.Position.DistanceTo(wsTarget.Position);


            DebugShapes.DrawLine(upperArmWS.Position, wsTarget.Position, Colors.Red);

            Vector3 fwaf = wsTarget.TransformDirection(Vector3.Right);
            Vector3 fwaf2 = Entity.Transform.TransformDirection(Vector3.Right);
            Vector3 cdir = Vector3.Lerp(fwaf, fwaf2, MathF.Pow(1f - MathFX.Saturate(fwaf.Dot(Vector3.Up)), 2.0f));
            Vector3 pvec = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), cdir);//wsTarget.TransformDirection(Vector3.Right);

            Quaternion upperRotation = Quaternion.LookAt(wsTarget.Position - upperArmWS.Position, pvec);
            upperRotation = Quaternion.AngleAxis(CosineRule(a, b, c), -pvec) * upperRotation;

            var newUpperArmWS = upperArmWS;
            newUpperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(upperArm.ID, newUpperArmWS, Entity.Transform);

            var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
            DebugShapes.DrawLine((upperArmWS.Position + wsTarget.Position) * 0.5f, lowerArmWS.Position, Colors.Red);

            Quaternion lowerRotation =
                Quaternion.LookAt(wsTarget.Position - lowerArmWS.Position, pvec);

            var newLowerarmWS = lowerArmWS;
            newLowerarmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(lowerArm.ID, newLowerarmWS, Entity.Transform);
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
            var lhb = mesh.GetBone("hand_R");
            var laT = swo.GetBoneTransform(lowerArm.ID);
            var hT = swo.GetBoneTransform(lhb.ID);

            swo.SetBoneWorldSpaceTransform(lhb.ID, targetTransform, Entity.Transform);

            float a = lowerArm.RestPose.Position.Length;
            float c = lhb.RestPose.Position.Length;

            var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);
            float b = upperArmWS.Position.DistanceTo(wsTarget.Position);


            DebugShapes.DrawLine(upperArmWS.Position, wsTarget.Position, Colors.Red);

            Vector3 fwaf = wsTarget.TransformDirection(Vector3.Left);
            Vector3 fwaf2 = Entity.Transform.TransformDirection(Vector3.Left);
            Vector3 cdir = Vector3.Lerp(fwaf, fwaf2, MathF.Pow(1f - MathFX.Saturate(fwaf.Dot(Vector3.Up)), 2.0f));
            Vector3 pvec = Vector3.Cross(upperArmWS.Position.DirectionTo(wsTarget.Position), cdir);//wsTarget.TransformDirection(Vector3.Right);

            Quaternion upperRotation = Quaternion.LookAt(wsTarget.Position - upperArmWS.Position, pvec);
            //upperRotation *= (Quaternion.FromTo(Vector3.Forward, laT.Position)).Inverse;
            upperRotation = Quaternion.AngleAxis(CosineRule(a, b, c), -pvec) * upperRotation;

            var newUpperArmWS = upperArmWS;
            newUpperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(upperArm.ID, newUpperArmWS, Entity.Transform);

            var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);
            DebugShapes.DrawLine((upperArmWS.Position + wsTarget.Position) * 0.5f, lowerArmWS.Position, Colors.Red);

            Quaternion lowerRotation =
                Quaternion.LookAt(wsTarget.Position - lowerArmWS.Position, pvec);
            //lowerRotation *= (Quaternion.FromTo(Vector3.Forward, hT.Position)).Inverse;

            var newLowerarmWS = lowerArmWS;
            newLowerarmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneWorldSpaceTransform(lowerArm.ID, newLowerarmWS, Entity.Transform);
        }
    }
}