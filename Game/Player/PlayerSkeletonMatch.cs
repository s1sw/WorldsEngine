using WorldsEngine;
using WorldsEngine.Math;

namespace Game.Player;

[Component]
public class PlayerSkeletonMatch : Component, IStartListener, IUpdateableComponent
{
    private Transform _initialT;
    private Transform _lhToWorld = new(Vector3.Zero, Quaternion.Identity);

    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        _initialT = swo.GetBoneTransform(root);

        var mesh = MeshManager.GetMesh(swo.Mesh);
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");

        int parentIdx = mesh.BoneParentIndex((int)lh);

        while (parentIdx != -1)
        {
            _lhToWorld = _lhToWorld.TransformBy(swo.GetBoneTransform((uint)parentIdx));
        }
    }
    public void Update()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        var root = MeshManager.GetBoneIndex(swo.Mesh, "root");
        var rt = MeshManager.GetBoneRestPose(swo.Mesh, root);
        swo.SetBoneTransform(MeshManager.GetBoneIndex(swo.Mesh, "root"), new Transform(Vector3.Down * 0.9f, _initialT.Rotation));
        var lh = MeshManager.GetBoneIndex(swo.Mesh, "hand_L");
        swo.SetBoneTransform(lh, LocalPlayerSystem.LeftHand.Transform.TransformByInverse(Entity.Transform).TransformByInverse(_lhToWorld));
    }
}