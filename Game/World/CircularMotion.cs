using WorldsEngine;
using WorldsEngine.Math;
using System;

namespace Game.World;

[Component]
class CircularMotion : Component, IThinkingComponent, IStartListener
{
    public float Radius = 5.0f;
    private Transform _initialTransform;
    private double _offset = 0.0f;

    public void Start()
    {
        _initialTransform = Registry.GetTransform(Entity);
        _offset = Random.Shared.NextDouble() * 5.0;
    }

    public void Think()
    {

        Transform nextT = _initialTransform;
        nextT.Position = _initialTransform.Position + new Vector3(
            (float)Math.Sin((Time.CurrentTime + _offset) * 2.0) * Radius,
            0.0f,
            (float)Math.Cos((Time.CurrentTime + _offset) * 2.0) * Radius
        );

        Registry.SetTransform(Entity, nextT);
    }
}
