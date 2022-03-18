using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace Game;

public class V3PidController
{
    [NonSerialized]
    private Vector3 lastError;
    [NonSerialized]
    private Vector3 integral;

    public float P = 0.0f;
    public float I = 0.0f;
    public float D = 0.0f;

    public bool ClampIntegral = false;
    public float MaxIntegralMagnitude = float.MaxValue;

    public float AverageAmount = 20.0f;

    private void CheckNaNs()
    {
        if (lastError.HasNaNComponent)
            lastError = Vector3.Zero;

        if (integral.HasNaNComponent)
            integral = Vector3.Zero;
    }

    public Vector3 CalculateForce(Vector3 error, float deltaTime, Vector3 referenceVelocity)
    {
        CheckNaNs();

        Vector3 derivative = ((error - lastError) / deltaTime) + referenceVelocity;
        integral += error * deltaTime;

        if (ClampIntegral)
        {
            integral = integral.ClampMagnitude(MaxIntegralMagnitude);
        }

        integral += (error - integral) / AverageAmount;

        lastError = error;

        return P * error + I * integral + D * derivative;
    }

    public Vector3 CalculateForce(Vector3 error, float deltaTime)
    {
        CheckNaNs();

        Vector3 derivative = ((error - lastError) / deltaTime);
        integral += error * deltaTime;

        if (ClampIntegral)
        {
            integral = integral.ClampMagnitude(MaxIntegralMagnitude);
        }

        integral += (error - integral) / AverageAmount;

        lastError = error;

        return P * error + I * integral + D * derivative;
    }
}
