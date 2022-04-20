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
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");
        var rh = MeshManager.GetBoneIndex(swo.Mesh, "hand_R");

        int parentIdx = mesh.BoneParentIndex((int)lh);

        while (parentIdx != -1)
        {
            _lhToWorld = _lhToWorld.TransformBy(swo.GetBoneTransform((uint)parentIdx));
            parentIdx = mesh.BoneParentIndex(parentIdx);
        }

        parentIdx = mesh.BoneParentIndex((int)rh);

        while (parentIdx != -1)
        {
            _rhToWorld = _rhToWorld.TransformBy(swo.GetBoneTransform((uint)parentIdx));
            parentIdx = mesh.BoneParentIndex(parentIdx);
        }
    }
    public void Update()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        var rootTransform = new Transform(Vector3.Down * 0.9f, _initialT.Rotation);
        swo.SetBoneTransform(MeshManager.GetBoneIndex(swo.Mesh, "root"), rootTransform);
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");
        var rh = MeshManager.GetBoneIndex(swo.Mesh, "hand_R");
        
        {
            var wsTarget = LocalPlayerSystem.LeftHand.Transform;
            wsTarget.Position += Vector3.Up * 0.9f;
            wsTarget.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneTransform(lh, wsTarget.TransformByInverse(Entity.Transform).TransformByInverse(_lhToWorld));
        }

        {
            var wsTarget = LocalPlayerSystem.RightHand.Transform;
            wsTarget.Position += Vector3.Up * 0.9f;
            wsTarget.Rotation *= new Quaternion(new Vector3(MathF.PI * 0.25f, 0f, MathF.PI * 0.25f));
            swo.SetBoneTransform(rh, wsTarget.TransformByInverse(Entity.Transform).TransformByInverse(_rhToWorld));
        }
    }
}