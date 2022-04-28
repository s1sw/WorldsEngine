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
    private readonly Vector3 _lowerDisplacement;


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

    public TwoBoneIK(float upperLength, float lowerLength, Vector3 lowerDisplacement)
    {
        UpperLength = upperLength;
        LowerLength = lowerLength;
        _lowerDisplacement = lowerDisplacement;
    }

    public TwoBoneIKSolveResult Solve(Transform upper, Transform lower, Transform target, Vector3 pole)
    {
        Log.Msg($"up: {upper.Position}");
        float a = UpperLength;
        float b = upper.Position.DistanceTo(target.Position);
        float c = LowerLength;

        Quaternion upperRotation = Quaternion.LookAt(target.Position - upper.Position, pole);
        upperRotation = Quaternion.AngleAxis(CosineRule(a, b, c), -pole) * upperRotation;

        lower.Position = upper.Position + (upperRotation * _lowerDisplacement);

        Quaternion lowerRotation = Quaternion.LookAt(target.Position - lower.Position, pole);

        return new TwoBoneIKSolveResult() {
            UpperRotation = upperRotation,
            LowerRotation = lowerRotation,
            LowerPosition = lower.Position
        };
    }
}