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

    float CosAngle(float a, float b, float c)
    {
        float val = MathF.Acos((-(c * c) + (a * a) + (b * b)) / (-2 * a * b));
        if (!float.IsNaN(val))
        {
            return val;
        }
        else
        {
            return 1;
        }
    }

    public void Update()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        var rootTransform = new Transform(Vector3.Down * 0.9f + Vector3.Backward * 0.25f, _initialT.Rotation);
        swo.SetBoneTransform(MeshManager.GetBoneIndex(swo.Mesh, "root"), rootTransform);
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");
        var rh = MeshManager.GetBoneIndex(swo.Mesh, "hand_R");

        {
            var wsTarget = LocalPlayerSystem.LeftHand.Transform;
            var targetTransform = wsTarget.TransformByInverse(Entity.Transform);
            // Root offset
            targetTransform.Position += Entity.Transform.TransformDirection(Vector3.Up * 0.9f + Vector3.Forward * 0.25f);
            // Fix rotation
            targetTransform.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            targetTransform = targetTransform.TransformByInverse(Entity.Transform);

            var mesh = MeshManager.GetMesh(swo.Mesh);
            var lowerArm = mesh.GetBone("lowerarm_L");
            var upperArm = mesh.GetBone("upperarm_L");
            var lhb = mesh.GetBone("hand_L");
            var laT = swo.GetBoneTransform(lowerArm.ID);
            var hT = swo.GetBoneTransform(lhb.ID);

            float a = laT.Position.Length;
            float b = hT.Position.Length;
            float c = swo.GetBoneComponentSpaceTransform(upperArm.ID).Position.DistanceTo(targetTransform.Position);

            var upperArmWS = swo.GetBoneComponentSpaceTransform(upperArm.ID).TransformBy(Entity.Transform);
            var lowerArmWS = swo.GetBoneComponentSpaceTransform(lowerArm.ID).TransformBy(Entity.Transform);

            DebugShapes.DrawLine(upperArmWS.Position, wsTarget.Position, Colors.Red);
            DebugShapes.DrawLine((upperArmWS.Position + wsTarget.Position) * 0.5f, lowerArmWS.Position, Colors.Red);

            Quaternion upperRotation = Quaternion.LookAt(wsTarget.Position - upperArmWS.Position, Vector3.Forward);
            upperRotation *= (Quaternion.FromTo(Vector3.Forward, laT.Position)).Inverse;
            upperRotation = Quaternion.AngleAxis(-CosAngle(a, c, b), -Vector3.Forward) * upperRotation;

            var newUpperArmWS = upperArmWS;
            newUpperArmWS.Rotation = upperRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            newUpperArmWS = newUpperArmWS.TransformByInverse(Entity.Transform);
            newUpperArmWS = newUpperArmWS.TransformByInverse(swo.GetBoneComponentSpaceTransform((uint)upperArm.Parent));
            swo.SetBoneTransform(upperArm.ID, newUpperArmWS);

            Quaternion lowerRotation =
                Quaternion.LookAt(wsTarget.Position - lowerArmWS.Position, Vector3.Forward);
            lowerRotation *= (Quaternion.FromTo(Vector3.Forward, hT.Position)).Inverse;

            var newLowerarmWS = lowerArmWS;
            newLowerarmWS.Rotation = lowerRotation * new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            newLowerarmWS = newLowerarmWS.TransformByInverse(Entity.Transform);
            newLowerarmWS = newLowerarmWS.TransformByInverse(swo.GetBoneComponentSpaceTransform((uint)lowerArm.Parent));
            swo.SetBoneTransform(lowerArm.ID, newLowerarmWS);
        }

        {
            var wsTarget = LocalPlayerSystem.RightHand.Transform;
            wsTarget.Position += Entity.Transform.TransformDirection(Vector3.Up * 0.9f + Vector3.Forward * 0.25f);
            wsTarget.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneTransform(rh, wsTarget.TransformByInverse(Entity.Transform).TransformByInverse(_rhToWorld));
        }
    }
}