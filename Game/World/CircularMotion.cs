using WorldsEngine;
using WorldsEngine.Math;
using System;

namespace Game.World;

[Component]
class CircularMotion : Component, IThinkingComponent, IStartListener
{
    static Random rng = new();
    private Transform _initialTransform;
    private double _offset = 0.0f;

    public void Start()
    {
        _initialTransform = Registry.GetTransform(Entity);
        _offset = rng.NextDouble() * 5.0;
    }

    public void Think()
    {
        const float Radius = 5.0f;

        WorldLight light = Registry.GetComponent<WorldLight>(Entity);
        light.Intensity = (float)Math.Sin(Time.CurrentTime + _offset) + 3.5f;
        Transform nextT = _initialTransform;
        nextT.Position = _initialTransform.Position + new Vector3(
            (float)Math.Sin((Time.CurrentTime + _offset) * 2.0) * Radius,
            0.0f,
            (float)Math.Cos((Time.CurrentTime + _offset) * 2.0) * Radius
        );

        Registry.SetTransform(Entity, nextT);
    }
}
