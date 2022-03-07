using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Util;

namespace Game;

[Component]
public class Turret : Component, IStartListener, IThinkingComponent
{
    private Transform _initialPitchTransform;
    private Transform _initialYawTransform;
    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();

        for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
            var restPose = MeshManager.GetBoneRestTransform(swo.Mesh, i);
            swo.SetBoneTransform(i, restPose);
        }

        _initialPitchTransform = MeshManager.GetBoneRestTransform(swo.Mesh, MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot"));
        _initialYawTransform = MeshManager.GetBoneRestTransform(swo.Mesh, MeshManager.GetBoneIndex(swo.Mesh, "YawPivot"));
    }

    public void Think()
    {
        var transform = Entity.Transform;
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        uint pitchBoneIdx = MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot");
        uint yawBoneIdx = MeshManager.GetBoneIndex(swo.Mesh, "YawPivot");

        Vector3 gunPos = transform.Position + new Vector3(0.0f, 0.875f, 0.0f);
        Vector3 target = Camera.Main.Position;
        float distance = gunPos.DistanceTo(target);
        Vector3 direction = (target - gunPos).Normalized;

        float pitchAngle = MathF.Asin(direction.y);
        float yawAngle = MathF.Atan2(direction.x, direction.z);

        Transform pitchPivotTransform = _initialPitchTransform; 
        Quaternion rotation = Quaternion.AngleAxis(pitchAngle - (MathF.PI * 0.5f), Vector3.Forward);
        pitchPivotTransform.Rotation = rotation;
        swo.SetBoneTransform(pitchBoneIdx, pitchPivotTransform);

        Transform yawPivotTransform = _initialYawTransform; 
        Quaternion yawRotation = Quaternion.AngleAxis(yawAngle - (MathF.PI * 0.5f), Vector3.Up);
        yawPivotTransform.Rotation = yawRotation;
        swo.SetBoneTransform(yawBoneIdx, yawPivotTransform);
    }
}
