using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Util
{
    public class InertiaTensorComputer
    {
        public Mat3x3 Inertia => _inertiaMatrix;
        public float Mass => _mass;
        private Mat3x3 _inertiaMatrix = new();
        private Vector3 _centerOfMass = new();
        private float _mass = 0.0f;

        public InertiaTensorComputer() { }

        public void Add(InertiaTensorComputer other)
        {
            float totalMass = _mass + other._mass;

            _centerOfMass = (_centerOfMass * _mass + other._centerOfMass * other._mass) / totalMass;
            _mass = totalMass;
            _inertiaMatrix += other._inertiaMatrix;
        }

        public void SetSphere(float radius)
        {
            float mass = (4.0f / 3.0f) * MathF.PI * radius * radius * radius;
            float s = mass * radius * radius * (2.0f / 5.0f);

            SetDiagonal(mass, new Vector3(s));
        }

        private float Volume(Vector3 extents)
        {
            float v = 1f;
            v *= extents.x != 0f ? extents.x : 1f;
            v *= extents.y != 0f ? extents.y : 1f;
            v *= extents.z != 0f ? extents.z : 1f;

            return v;
        }

        public void SetBox(Vector3 halfExtents)
        {
            float mass = 8.0f * Volume(halfExtents);
            float s = (1.0f / 3.0f) * mass;

            float x = halfExtents.x * halfExtents.x;
            float y = halfExtents.y * halfExtents.y;
            float z = halfExtents.z * halfExtents.z;

            SetDiagonal(mass, new Vector3(y + z, z + x, x + y) * s);
        }

        public void Translate(Vector3 t)
        {
            if (t.IsZero) return;

            Mat3x3 t1 = new Mat3x3(
                new Vector3(0.0f, _centerOfMass.z, _centerOfMass.y),
                new Vector3(-_centerOfMass.z, 0.0f, _centerOfMass.x),
                new Vector3(_centerOfMass.y, -_centerOfMass.x, 0.0f)
            );

            Vector3 sum = _centerOfMass + t;
            if (sum.IsZero)
            {
                _inertiaMatrix = (t1 * t1) * _mass;
            }
            else
            {
                Mat3x3 t2 = new Mat3x3(
                    new Vector3(0.0f, sum.z, -sum.y),
                    new Vector3(-sum.z, 0.0f, sum.x),
                    new Vector3(sum.y, -sum.x, 0.0f)
                );
                _inertiaMatrix += (t1 * t1 - t2 * t2) * _mass;
            }

            _centerOfMass += t;
        }

        public void Rotate(Mat3x3 rotation)
        {
            _inertiaMatrix = rotation * _inertiaMatrix * rotation.Transpose;
            _centerOfMass = rotation.Transform(_centerOfMass);
        }

        public void Rotate(Quaternion rotation)
        {
            Rotate((Mat3x3)rotation);
        }

        public void ScaleDensity(float densityScale)
        {
            _inertiaMatrix *= densityScale;
            _mass *= densityScale;
        }

        private void SetDiagonal(float mass, Vector3 diag)
        {
            _mass = mass;
            _inertiaMatrix = new Mat3x3(
                new Vector3(diag.x, 0.0f, 0.0f),
                new Vector3(0.0f, diag.y, 0.0f),
                new Vector3(0.0f, 0.0f, diag.z)
            );
        }
    }
}
