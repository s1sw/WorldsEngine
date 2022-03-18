using WorldsEngine;
using WorldsEngine.ECS;

namespace Game.Player;

[Component]
class HandSkeleton : Component, IStartListener, IUpdateableComponent
{
    public bool IsRightHand = false;

    private int _boneCount;

    public void Start()
    {
        SkinnedWorldObject swo = Entity.GetComponent<SkinnedWorldObject>();
        _boneCount = MeshManager.GetBoneCount(swo.Mesh);
    }

    public void Update()
    {
        if (!VR.Enabled) return;

        SkinnedWorldObject swo = Entity.GetComponent<SkinnedWorldObject>();

        BoneTransforms bt = IsRightHand ? VR.RightHandBones : VR.LeftHandBones;
        for (int i = 0; i < _boneCount; i++) {
            swo.SetBoneTransform((uint)i, bt[i]);
        }
    }
}