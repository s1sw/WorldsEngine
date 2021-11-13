using WorldsEngine;
using WorldsEngine.Math;
using System;

namespace Game
{
    [Component]
    class CircularMotion : IThinkingComponent, IStartListener
    {
        static Random rng = new();
        private Transform _initialTransform;
        private double _offset = 0.0f;

        public void Start(Entity e)
        {
            _initialTransform = Registry.GetTransform(e);
            _offset = rng.NextDouble() * 5.0;
        }

        public void Think(Entity e)
        {
            const float Radius = 5.0f;

            WorldLight light = Registry.GetComponent<WorldLight>(e);
            light.Intensity = (float)Math.Sin(Time.CurrentTime + _offset) + 3.5f;
            Transform nextT = _initialTransform;
            nextT.Position = _initialTransform.Position + new Vector3(
                (float)Math.Sin((Time.CurrentTime + _offset) * 2.0) * Radius,
                0.0f,
                (float)Math.Cos((Time.CurrentTime + _offset) * 2.0) * Radius
            );

            Registry.SetTransform(e, nextT);
        }
    }
}
