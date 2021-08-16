using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace Game
{
    public class StablePD
    {
        [NonSerialized]
        private Vector3 lastVelocity = Vector3.Zero;

        public float P = 0.0f;
        public float D = 0.0f;

        public StablePD() { }

        public Vector3 CalculateForce(Vector3 currentPosition, Vector3 desiredPosition, Vector3 velocity, float deltaTime)
        {
            if (lastVelocity.HasNaNComponent)
                lastVelocity = Vector3.Zero;

            Vector3 acceleration = velocity - lastVelocity;

            Vector3 result = -P * (currentPosition + (deltaTime * velocity) - desiredPosition)
                - D * (velocity + (deltaTime * acceleration));

            lastVelocity = velocity;

            return result;
        }

        public void Reset()
        {
            lastVelocity = Vector3.Zero;
        }
    }
}
