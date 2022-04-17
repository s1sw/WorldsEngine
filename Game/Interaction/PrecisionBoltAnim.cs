using WorldsEngine;
using WorldsEngine.Math;
using System;

namespace Game.Interaction;

[Component]
public class PrecisionBoltAnim : Component, IStartListener, IUpdateableComponent
{
    private float _currentSpeed = 0.0f;

    public void Start()
    {
        Entity.GetComponent<Gun>().OnFire += () => _currentSpeed = 0.0f;
    }

    public void Update()
    {
        var grabbable = Entity.GetComponent<Grabbable>();
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        if (grabbable.AttachedHandFlags != AttachedHandFlags.None)
        {
            _currentSpeed += Time.DeltaTime * 7f;
        }
        else
        {
            _currentSpeed -= Time.DeltaTime * 7f;
        }

        _currentSpeed = MathFX.Clamp(_currentSpeed, 0.0f, 10.0f);

        uint spinIdx = MeshManager.GetBoneIndex(swo.Mesh, "SpinningDiscs");
        Transform t = swo.GetBoneTransform(spinIdx);
        t.Rotation *= Quaternion.AngleAxis(MathF.PI * Time.DeltaTime * _currentSpeed, Vector3.Left);
        swo.SetBoneTransform(spinIdx, t);
    }
}