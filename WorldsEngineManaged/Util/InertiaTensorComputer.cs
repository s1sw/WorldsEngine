// NOTE: This was not written by me! This is a more or less direct port of PhysX's InertiaTensorComputer class
// to C#.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Util
{
    /// <summary>
    /// Inertia tensor computer ported from PhysX.
    /// </summary>
    /// <example>
    /// To compute the inertia tensor of a 5kg box with half extents (0.5, 0.5, 0.5):
    /// <code>
    /// InertiaTensorComputer itc = new InertiaTensorComputer();
    /// itc.SetBox(new Vector3(0.5f));
    /// itc.ScaleDensity(5.0f / itc.Mass);
    /// </code>
    /// 
    /// The resulting inertia tensor and center of mass can either be used directly,
    /// or combined with another InertiaTensorComputer.
    /// 
    /// The <see cref="Translate(Vector3)"/> and <see cref="Rotate(Quaternion)"/> methods
    /// can be used to offset the computed inertia tensor.
    /// </example>
    /// <remarks>
    /// This code uses my math library for Worlds Engine. To port to Unity, you'll need to find a suitable
    /// *column-major* 3x3 matrix type. The Unity.Mathematics float3x3 type should work. Note that you
    /// may also need to change Vector3 constructors that take a single float value (x) to (x, x, x).
    /// </remarks>
    public class InertiaTensorComputer
    {
        public Mat3x3 InertiaTensor => _inertiaMatrix;
        public float Mass => _mass;
        public Vector3 CenterOfMass => _centerOfMass;

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

        public enum CapsuleAxis
        {
            X,
            Y,
            Z
        }

        public void SetCapsule(CapsuleAxis axis, float radius, float length)
        {
            float mass = SphereVolume(radius) + CylinderVolume(radius, length);

            float t = MathF.PI * radius * radius;
            float i1 = t * ((radius * radius * radius * 8.0f / 15.0f) + (length * radius * radius));
            float i2 = t * ((radius * radius * radius * 8.0f / 15.0f) +
                (length * radius * radius * 3.0f / 2.0f) + 
                (length * radius * radius * 4.0f / 3.0f) + 
                (length * length * length * 2.0f / 3.0f));

            switch (axis)
            {
                case CapsuleAxis.X:
                    SetDiagonal(mass, new Vector3(i1, i2, i2));
                    break;
                case CapsuleAxis.Y:
                    SetDiagonal(mass, new Vector3(i2, i1, i2));
                    break;
                case CapsuleAxis.Z:
                    SetDiagonal(mass, new Vector3(i2, i2, i1));
                    break;
            }
        }

        public void SetBox(Vector3 halfExtents)
        {
            float mass = 8.0f * BoxVolume(halfExtents);
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

        private float BoxVolume(Vector3 extents)
        {
            float v = 1f;
            v *= extents.x != 0f ? extents.x : 1f;
            v *= extents.y != 0f ? extents.y : 1f;
            v *= extents.z != 0f ? extents.z : 1f;

            return v;
        }

        private float SphereVolume(float radius)
        {
            return (4.0f / 3.0f) * MathF.PI * (radius * radius * radius);
        }

        private float CylinderVolume(float radius, float length)
        {
            return MathF.PI * (radius * radius) * length * 2.0f;
        }
    }
}
