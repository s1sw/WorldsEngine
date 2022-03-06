using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Util;

namespace Game;

[Component]
public class Turret : Component, IStartListener, IThinkingComponent
{
    private Transform _initialPitchTransform;
    public void Start()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();

        for (uint i = 0; i < MeshManager.GetBoneCount(swo.Mesh); i++) {
            var restPose = MeshManager.GetBoneRestTransform(swo.Mesh, i);
            swo.SetBoneTransform(i, restPose);
        }

        _initialPitchTransform = MeshManager.GetBoneRestTransform(swo.Mesh, MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot"));
    }

    public void Think()
    {
        var swo = Entity.GetComponent<SkinnedWorldObject>();
        uint pitchBoneIdx = MeshManager.GetBoneIndex(swo.Mesh, "PitchPivot");

        Transform pitchPivotTransform = _initialPitchTransform; 
        float rotationT = (float)Math.Sin(Time.CurrentTime) * 0.5f + 0.5f;
        Quaternion rotation = Quaternion.AngleAxis(rotationT * MathF.PI - (MathF.PI / 2.0f), Vector3.Forward);
        pitchPivotTransform.Rotation = rotation;
        swo.SetBoneTransform(pitchBoneIdx, pitchPivotTransform);
    }
}
