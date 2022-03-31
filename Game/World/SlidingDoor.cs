using WorldsEngine;
using WorldsEngine.Audio;
using WorldsEngine.Math;
using WorldsEngine.Editor;
using ImGuiNET;
using System;

namespace Game.World;

[Component]
[CustomEditor(typeof(Editors.SlidingDoorEditor))]
public class SlidingDoor : Component, IStartListener, IThinkingComponent
{
    public Vector3 SlideAxis;
    public float SlideDistance;
    public Vector3 TriggerSize;
    public float Speed = 1f;

    private float _slideT = 0f;
    private int _previousSlideDir = 0;
    private Transform _initialTransform;

    public void Start()
    {
        _initialTransform = Entity.Transform;
    }

    public float EaseInOutQuad(float x)
    {
        return x < 0.5f ? 2f * x * x : 1f - MathF.Pow(-2f * x + 2f, 2f) / 2f;
    }

    public void Think()
    {
        Transform t = Entity.Transform;
        AABB aabb = new(TriggerSize);

        if (aabb.ContainsPoint(_initialTransform.InverseTransformPoint(Player.PlayerCameraSystem.WorldSpaceHeadPosition)))
        {
            if (_previousSlideDir == -1)
                Audio.PlayOneShotAttachedEvent("event:/Misc/Sliding Door", t.Position, Entity);
            _slideT += Time.DeltaTime * Speed;
            _previousSlideDir = 1;
        }
        else
        {
            if (_previousSlideDir == 1)
                Audio.PlayOneShotAttachedEvent("event:/Misc/Sliding Door", t.Position, Entity);
            _slideT -= Time.DeltaTime * Speed;
            _previousSlideDir = -1;
        }

        _slideT = MathFX.Clamp(_slideT, 0f, 1f);

        ImGui.Text($"_slideT: {_slideT}");

        t.Position = Vector3.Lerp(_initialTransform.Position, _initialTransform.Position + SlideAxis * SlideDistance, EaseInOutQuad(_slideT));
        Registry.SetTransform(Entity, t);
    }
}