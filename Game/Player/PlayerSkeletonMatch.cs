using System;
using WorldsEngine;
using WorldsEngine.Math;

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
            wsTarget.Position += Entity.Transform.TransformDirection(Vector3.Up * 0.9f + Vector3.Forward * 0.25f);
            wsTarget.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneTransform(lh, wsTarget.TransformByInverse(Entity.Transform).TransformByInverse(_lhToWorld));
        }

        {
            var wsTarget = LocalPlayerSystem.RightHand.Transform;
            wsTarget.Position += Entity.Transform.TransformDirection(Vector3.Up * 0.9f + Vector3.Forward * 0.25f);
            wsTarget.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneTransform(rh, wsTarget.TransformByInverse(Entity.Transform).TransformByInverse(_rhToWorld));
        }
    }
}