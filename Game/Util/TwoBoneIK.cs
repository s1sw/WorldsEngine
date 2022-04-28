using System;
using WorldsEngine;
using WorldsEngine.Math;

namespace Game.Util;

public struct TwoBoneIKSolveResult
{
    public Quaternion UpperRotation;
    public Quaternion LowerRotation;
    public Vector3 LowerPosition;
}

public class TwoBoneIK
{
    public readonly float UpperLength;
    public readonly float LowerLength;


    //  __b___
    //  \<v  /
    //   a  c
    //    \/ 
    //
    private static float CosineRule(float a, float b, float c)
    {
        float v = MathF.Acos(((a * a) + (b * b) - (c * c)) / (2 * a * b));

        return float.IsFinite(v) ? v : 0.0f;
    }

    public TwoBoneIK(float upperLength, float lowerLength)
    {
        UpperLength = upperLength;
        LowerLength = lowerLength;
    }

    public TwoBoneIKSolveResult Solve(Transform upper, Transform lower, Transform target, Vector3 pole)
    {
        float a = UpperLength;
        float b = upper.Position.DistanceTo(target.Position);
        float c = LowerLength;

        Vector3 upperOffset = upper.Position.VectorTo(lower.Position);

        Quaternion upperRotation = Quaternion.LookAt(target.Position - upper.Position, pole);
        upperRotation = Quaternion.AngleAxis(CosineRule(a, b, c), -pole) * upperRotation;

        lower.Position = upper.Position + (upperRotation * upperOffset);

        Quaternion lowerRotation = Quaternion.LookAt(target.Position - lower.Position, pole);

        return new TwoBoneIKSolveResult() {
            UpperRotation = upperRotation,
            LowerRotation = lowerRotation,
            LowerPosition = lower.Position
        };
    }
}