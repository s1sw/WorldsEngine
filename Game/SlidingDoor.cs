using WorldsEngine;
using WorldsEngine.Math;
using ImGuiNET;
using System;

namespace Game;

[Component]
public class SlidingDoor : Component, IStartListener, IThinkingComponent
{
    public Vector3 SlideAxis;
    public float SlideDistance;
    public Vector3 TriggerSize;
    public float Speed = 1f;

    private float _slideT = 0f;
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

        if (aabb.ContainsPoint(_initialTransform.InverseTransformPoint(Camera.Main.Position)))
        {
            _slideT += Time.DeltaTime * Speed;
        }
        else
        {
            _slideT -= Time.DeltaTime * Speed;
        }

        _slideT = MathFX.Clamp(_slideT, 0f, 1f);

        ImGui.Text($"_slideT: {_slideT}");

        t.Position = Vector3.Lerp(_initialTransform.Position, _initialTransform.Position + SlideAxis * SlideDistance, EaseInOutQuad(_slideT));
        Registry.SetTransform(Entity, t);
    }
}